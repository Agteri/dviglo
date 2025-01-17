// Copyright (c) 2008-2023 the Urho3D project
// Copyright (c) 2022-2023 the Dviglo project
// License: MIT

#include "../../graphics/graphics.h"
#include "../graphics_impl.h"
#include "ogl_shader_program.h"
#include "../shader.h"
#include "../shader_variation.h"
#include "../../io/log.h"

#include "../../common/debug_new.h"

namespace dviglo
{

const char* ShaderVariation::elementSemanticNames_OGL[] =
{
    "POS",
    "NORMAL",
    "BINORMAL",
    "TANGENT",
    "TEXCOORD",
    "COLOR",
    "BLENDWEIGHT",
    "BLENDINDICES",
    "OBJECTINDEX"
};

void ShaderVariation::OnDeviceLost_OGL()
{
    if (object_.name_ && !DV_GRAPHICS.IsDeviceLost())
        glDeleteShader(object_.name_);

    GPUObject::OnDeviceLost();

    compilerOutput_.Clear();
}

void ShaderVariation::Release_OGL()
{
    if (object_.name_)
    {
        if (GParams::is_headless())
            return;

        Graphics& graphics = DV_GRAPHICS;

        if (!graphics.IsDeviceLost())
        {
            if (type_ == VS)
            {
                if (graphics.GetVertexShader() == this)
                    graphics.SetShaders(nullptr, nullptr);
            }
            else
            {
                if (graphics.GetPixelShader() == this)
                    graphics.SetShaders(nullptr, nullptr);
            }

            glDeleteShader(object_.name_);
        }

        object_.name_ = 0;
        graphics.CleanupShaderPrograms_OGL(this);
    }

    compilerOutput_.Clear();
}

bool ShaderVariation::Create_OGL()
{
    Release_OGL();

    if (!owner_)
    {
        compilerOutput_ = "Owner shader has expired";
        return false;
    }

    object_.name_ = glCreateShader(type_ == VS ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    if (!object_.name_)
    {
        compilerOutput_ = "Could not create shader object";
        return false;
    }

    const String& originalShaderCode = owner_->GetSourceCode(type_);
    String shaderCode;

    // Check if the shader code contains a version define
    i32 verStart = originalShaderCode.Find('#');
    i32 verEnd = 0;
    if (verStart != String::NPOS)
    {
        if (originalShaderCode.Substring(verStart + 1, 7) == "version")
        {
            verEnd = verStart + 9;
            while (verEnd < originalShaderCode.Length())
            {
                if (IsDigit((unsigned)originalShaderCode[verEnd]))
                    ++verEnd;
                else
                    break;
            }
            // If version define found, insert it first
            String versionDefine = originalShaderCode.Substring(verStart, verEnd - verStart);
            shaderCode += versionDefine + "\n";
        }
    }
    // Force GLSL version 150 if no version define and GL3 is being used
    if (!verEnd)
    {
#if defined(MOBILE_GRAPHICS) || DV_GLES3
        shaderCode += "#version 300 es\n";
#else
        shaderCode += "#version 150\n";
#endif
    }
#if defined(DESKTOP_GRAPHICS)
    shaderCode += "#define DESKTOP_GRAPHICS\n";
#elif defined(MOBILE_GRAPHICS)
    shaderCode += "#define MOBILE_GRAPHICS\n";
#endif

    // Distinguish between VS and PS compile in case the shader code wants to include/omit different things
    shaderCode += type_ == VS ? "#define COMPILEVS\n" : "#define COMPILEPS\n";

    // Add define for the maximum number of supported bones
    shaderCode += "#define MAXBONES " + String(Graphics::GetMaxBones()) + "\n";

    // Prepend the defines to the shader code
    Vector<String> defineVec = defines_.Split(' ');
    for (unsigned i = 0; i < defineVec.Size(); ++i)
    {
        // Add extra space for the checking code below
        String defineString = "#define " + defineVec[i].Replaced('=', ' ') + " \n";
        shaderCode += defineString;

        // In debug mode, check that all defines are referenced by the shader code
#ifdef _DEBUG
        String defineCheck = defineString.Substring(8, defineString.Find(' ', 8) - 8);
        if (originalShaderCode.Find(defineCheck) == String::NPOS)
            DV_LOGWARNING("Shader " + GetFullName() + " does not use the define " + defineCheck);
#endif
    }

#ifdef RPI
    if (type_ == VS)
        shaderCode += "#define RPI\n";
#endif
#ifdef __EMSCRIPTEN__
    shaderCode += "#define WEBGL\n";
#endif
    shaderCode += "#define GL3\n";

    // When version define found, do not insert it a second time
    if (verEnd > 0)
        shaderCode += (originalShaderCode.c_str() + verEnd);
    else
        shaderCode += originalShaderCode;

    const char* shaderCStr = shaderCode.c_str();
    glShaderSource(object_.name_, 1, &shaderCStr, nullptr);
    glCompileShader(object_.name_);

    int compiled, length;
    glGetShaderiv(object_.name_, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        glGetShaderiv(object_.name_, GL_INFO_LOG_LENGTH, &length);
        compilerOutput_.Resize((unsigned)length);
        int outLength;
        glGetShaderInfoLog(object_.name_, length, &outLength, &compilerOutput_[0]);
        glDeleteShader(object_.name_);
        object_.name_ = 0;
    }
    else
        compilerOutput_.Clear();

    return object_.name_ != 0;
}

void ShaderVariation::SetDefines_OGL(const String& defines)
{
    defines_ = defines;
}

}
