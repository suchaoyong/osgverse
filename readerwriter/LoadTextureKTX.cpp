#include <osg/io_utils>
#include <osg/Version>
#include <osg/AnimationPath>
#include <osg/Texture2D>
#include <osg/Geometry>
#include <osgDB/ConvertUTF>
#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include "pipeline/Utilities.h"

#include <ktx/texture.h>
#include <ktx/gl_format.h>
#include "LoadTextureKTX.h"

namespace osgVerse
{
    static osg::ref_ptr<osg::Image> loadImageFromKtx(ktxTexture* texture, int layer, int face,
                                                     ktx_size_t imgDataSize)
    {
        ktx_uint32_t w = texture->baseWidth, h = texture->baseHeight, d = texture->baseDepth;
        ktx_size_t offset = 0; ktxTexture_GetImageOffset(texture, 0, layer, face, &offset);
        ktx_uint8_t* imgData = ktxTexture_GetData(texture) + offset;

        osg::ref_ptr<osg::Image> image = new osg::Image;
        if (texture->classId == ktxTexture2_c)
        {
            ktxTexture2* tex = (ktxTexture2*)texture;
            GLint glInternalformat = glGetInternalFormatFromVkFormat((VkFormat)tex->vkFormat);
            GLenum glFormat = glGetFormatFromInternalFormat(glInternalformat);
            GLenum glType = glGetTypeFromInternalFormat(glInternalformat);
            if (glFormat == GL_INVALID_VALUE || glType == GL_INVALID_VALUE)
            {
                OSG_WARN << "[LoaderKTX] Failed to get KTX2 file format\n";
                ktxTexture_Destroy(texture); return NULL;
            }
            image->allocateImage(w, h, d, glFormat, glType, 1);
            image->setInternalTextureFormat(glInternalformat);
        }
        else
        {
            ktxTexture1* tex = (ktxTexture1*)texture;
            image->allocateImage(w, h, d, tex->glFormat, tex->glType, 4);
            image->setInternalTextureFormat(tex->glInternalformat);
        }

        memcpy(image->data(), imgData, imgDataSize);
        return image;
    }

    static std::vector<osg::ref_ptr<osg::Image>> loadKtxFromObject(ktxTexture* texture)
    {
        std::vector<osg::ref_ptr<osg::Image>> resultArray;
        ktx_uint32_t numLevels = texture->numLevels;
        ktx_size_t imgDataSize = 0;
        if (texture->classId == ktxTexture2_c)
            imgDataSize = ktxTexture_calcImageSize(texture, 0, KTX_FORMAT_VERSION_TWO);
        else
            imgDataSize = ktxTexture_calcImageSize(texture, 0, KTX_FORMAT_VERSION_ONE);

        if (texture->isArray || texture->numFaces > 1)
        {
            if (texture->isArray)
            {
                for (ktx_uint32_t i = 0; i < texture->numLayers; ++i)
                {
                    osg::ref_ptr<osg::Image> image = loadImageFromKtx(texture, i, 0, imgDataSize);
                    if (image.valid()) resultArray.push_back(image);
                }
            }
            else
            {
                for (ktx_uint32_t i = 0; i < texture->numFaces; ++i)
                {
                    osg::ref_ptr<osg::Image> image = loadImageFromKtx(texture, 0, i, imgDataSize);
                    if (image.valid()) resultArray.push_back(image);
                }
            }
        }
        else
        {
            osg::ref_ptr<osg::Image> image = loadImageFromKtx(texture, 0, 0, imgDataSize);
            if (image.valid()) resultArray.push_back(image);
        }
        ktxTexture_Destroy(texture);
        return resultArray;
    }

    std::vector<osg::ref_ptr<osg::Image>> loadKtx(const std::string& file)
    {
        ktxTexture* texture = NULL;
        ktx_error_code_e result = ktxTexture_CreateFromNamedFile(
            file.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
        if (result != KTX_SUCCESS)
        {
            OSG_WARN << "[LoaderKTX] Unable to read from KTX file: " << file << "\n";
            return std::vector<osg::ref_ptr<osg::Image>>();
        }
        return loadKtxFromObject(texture);
    }

    std::vector<osg::ref_ptr<osg::Image>> loadKtx2(std::istream& in)
    {
        std::string data((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        if (data.empty()) return std::vector<osg::ref_ptr<osg::Image>>();

        ktxTexture* texture = NULL;
        ktx_error_code_e result = ktxTexture_CreateFromMemory(
            (const ktx_uint8_t*)data.data(), data.size(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
        if (result != KTX_SUCCESS)
        {
            OSG_WARN << "[LoaderKTX] Unable to read from KTX stream\n";
            return std::vector<osg::ref_ptr<osg::Image>>();
        }
        return loadKtxFromObject(texture);
    }

    static ktxTexture1* saveImageToKtx(const std::vector<osg::Image*>& images, bool asCubeMap)
    {
        ktxTexture1* texture = NULL;
        if (images.empty()) return NULL;
        if (asCubeMap && images.size() < 6) return NULL;

        osg::Image* image0 = images[0];
        ktxTextureCreateInfo createInfo;

        createInfo.glInternalformat = image0->getInternalTextureFormat();
        createInfo.baseWidth = image0->s();
        createInfo.baseHeight = image0->t();
        createInfo.baseDepth = image0->r();
        createInfo.numDimensions = (image0->r() > 1) ? 3 : 2;
        createInfo.numLevels = 1;
        createInfo.numLayers = asCubeMap ? 1 : images.size();
        createInfo.numFaces = asCubeMap ? images.size() : 1;
        createInfo.isArray = (images.size() > 1) ? KTX_TRUE : KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;

        KTX_error_code result = ktxTexture1_Create(
            &createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
        if (result != KTX_SUCCESS)
        {
            OSG_WARN << "[LoaderKTX] Unable to create KTX for saving\n";
            return NULL;
        }

        for (size_t i = 0; i < images.size(); ++i)
        {
            osg::Image* img = images[i];
            if (img->s() != image0->s() || img->t() != image0->t() ||
                img->getInternalTextureFormat() != image0->getInternalTextureFormat())
            {
                OSG_WARN << "[LoaderKTX] Mismatched image format while saving "
                    << "KTX texture\n"; continue;
            }

            ktx_uint8_t* src = (ktx_uint8_t*)img->data();
            result = ktxTexture_SetImageFromMemory(
                ktxTexture(texture), 0, (asCubeMap ? 0 : i), (asCubeMap ? i : 0),
                src, img->getTotalSizeInBytes());
            if (result != KTX_SUCCESS)
            {
                OSG_WARN << "[LoaderKTX] Unable to save image " << i
                    << " to KTX texture\n";
                ktxTexture_Destroy(ktxTexture(texture)); return NULL;
            }
        }
        return texture;
    }

    bool saveKtx(const std::string& file, bool asCubeMap,
                 const std::vector<osg::Image*>& images)
    {
        ktxTexture1* texture = saveImageToKtx(images, asCubeMap);
        if (texture == NULL) return false;

        KTX_error_code result = ktxTexture_WriteToNamedFile(ktxTexture(texture), file.c_str());
        ktxTexture_Destroy(ktxTexture(texture));
        return result == KTX_SUCCESS;
    }

    bool saveKtx2(std::ostream& out, bool asCubeMap,
                  const std::vector<osg::Image*>& images)
    {
        ktxTexture1* texture = saveImageToKtx(images, asCubeMap);
        if (texture == NULL) return false;

        ktx_uint8_t* buffer = NULL; ktx_size_t length = 0;
        KTX_error_code result = ktxTexture_WriteToMemory(
            ktxTexture(texture), &buffer, &length);
        if (result == KTX_SUCCESS)
        {
            out.write((char*)buffer, length);
            delete buffer;
        }
        ktxTexture_Destroy(ktxTexture(texture));
        return result == KTX_SUCCESS;
    }
}
