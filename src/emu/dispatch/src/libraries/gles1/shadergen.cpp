/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <dispatch/libraries/gles1/shadergen.h>
#include <fmt/format.h>
#include <common/log.h>

namespace eka2l1::dispatch {
    std::string generate_gl_vertex_shader(const std::uint64_t vertex_statuses, const std::uint32_t active_texs, const bool is_es) {
        std::string input_decl = "";
        std::string uni_decl = "";
        std::string out_decl = "";
        std::string main_body = "";

        bool skinning = vertex_statuses & egl_context_es1::VERTEX_STATE_SKINNING_ENABLE;

        if (is_es) {
            input_decl += "#version 300 es\n"
                          "precision highp float;\n";
        } else {
            input_decl += "#version 140\n"
                "#extension GL_ARB_explicit_attrib_location : require\n";
        }

        input_decl += "layout (location = 0) in vec4 inPosition;\n";

        if (skinning) {
            uni_decl += fmt::format("uniform mat4 uPaletteMat[{}];\n", GLES1_EMU_MAX_PALETTE_MATRICES);
        } else {
            uni_decl += "uniform mat4 uViewModelMat;\n";
        }

        uni_decl += "uniform mat4 uProjMat;\n";
        out_decl += "out vec4 mMyPos;\n";

        main_body += "void main() {\n";
        if (skinning) {
            input_decl += fmt::format("layout (location = {}) in uint uMatrixIndices[{}];\n", 3 + GLES1_EMU_MAX_TEXTURE_COUNT, GLES1_EMU_MAX_WEIGHTS_PER_VERTEX);
            input_decl += fmt::format("layout (location = {}) in float uWeights[{}];\n", 4 + GLES1_EMU_MAX_TEXTURE_COUNT, GLES1_EMU_MAX_WEIGHTS_PER_VERTEX);

            std::uint32_t weights_per_vertex_count = (vertex_statuses & egl_context_es1::VERTEX_STATE_SKIN_WEIGHTS_PER_VERTEX_MASK) 
                >> egl_context_es1::VERTEX_STATE_SKIN_WEIGHTS_PER_VERTEX_BITS_POS;

            main_body += "\tmat4 uViewModelMat = (";
            for (std::uint32_t i = 0; i < weights_per_vertex_count; i++) {
                main_body += "uWeights[{}] * uPaletteMat[uMatrixIndicies[{}]] ";
                if (i != weights_per_vertex_count - 1) {
                    main_body += "+ ";
                }
                main_body += ");\n";
            }
        }

        main_body += "\tgl_Position = uProjMat * uViewModelMat * inPosition;\n"
                     "\tmMyPos = uViewModelMat * inPosition;\n";

        if (vertex_statuses & egl_context_es1::VERTEX_STATE_CLIENT_COLOR_ARRAY) {
            input_decl += "layout (location = 1) in vec4 inColor;\n";
            main_body += "\tmColor = inColor;\n";
        } else {
            uni_decl += "uniform vec4 uColor;\n";
            main_body += "\tmColor = uColor;\n";
        }
        
        out_decl += "out vec4 mColor;\n";

        if (vertex_statuses & egl_context_es1::VERTEX_STATE_CLIENT_NORMAL_ARRAY) {
            input_decl += "layout (location = 2) in vec3 inNormal;\n";
            main_body += "\tmNormal = inNormal;\n";
        } else {
            input_decl += "uniform vec3 uNormal;\n";
            main_body += "\tmNormal = uNormal;\n";
        }

        main_body += "\tmat3 modelViewTrInv = mat3(inverse(transpose(uViewModelMat)));\n"
                     "\tmNormal = modelViewTrInv * mNormal;\n";

        if (vertex_statuses & egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_RESCALE) {
            // This bit is from ANGLE's renderer T_T
            main_body += "\tfloat lenRescale = length(vec3(modelViewTrInv[2]));\n"
                         "\tfloat rescale = 1.0;\n"
                         "\tif (lenRescale > 0.0) rescale = 1.0 / lenRescale;\n"
                         "\tmNormal *= rescale;\n";
        }

        if (vertex_statuses & egl_context_es1::VERTEX_STATE_NORMAL_ENABLE_NORMALIZE) {
            main_body += "\tmNormal = normalize(mNormal);\n";
        }

        out_decl += "out vec3 mNormal;\n";

        for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
            out_decl += fmt::format("out vec4 mTexCoord{};\n", i);
            uni_decl += fmt::format("uniform mat4 uTextureMat{};\n", i);
            
            if (active_texs & (1 << i)) {
                if (vertex_statuses & (1 << (egl_context_es1::VERTEX_STATE_CLIENT_TEXCOORD_ARRAY_POS + static_cast<std::uint8_t>(i)))) {
                    input_decl += fmt::format("layout (location = {}) in vec4 inTexCoord{};\n", 3 + i, i);
                    main_body += fmt::format("\tmTexCoord{} = uTextureMat{} * inTexCoord{};\n", i, i, i);
                } else {
                    uni_decl += fmt::format("uniform vec4 uTexCoord{};\n", i);
                    main_body += fmt::format("\tmTexCoord{} = uTextureMat{} * uTexCoord{};\n", i, i, i);
                }
            }
        }

        main_body += "}";

        return input_decl + uni_decl + out_decl + main_body;
    }

    static std::string generate_tex_env_gl_source_string(const std::size_t current_tex_index, const std::uint64_t source) {
        switch (source) {
        // Note: They are probably asking about the original source color in the buffer...
        // Need to check again...
        case gles_texture_env_info::SOURCE_TYPE_PRIM_COLOR:
            return "mColor";

        case gles_texture_env_info::SOURCE_TYPE_CONSTANT:
            return fmt::format("uTextureEnvColor{}", current_tex_index);

        case gles_texture_env_info::SOURCE_TYPE_PREVIOUS:
            return "oColor";

        case gles_texture_env_info::SOURCE_TYPE_CURRENT_TEXTURE:
            return fmt::format("pixelTex{}", current_tex_index);

        case gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_0:
            return "pixelTex0";

        case gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_1:
            return "pixelTex1";

        case gles_texture_env_info::SOURCE_TYPE_TEXTURE_STAGE_2:
            return "pixelTex2";

        default:
            break;
        }

        return "iWasNotSupposedToReachHereButIWillJustBeHereToThrowThisShaderToHellMakingItInvalid";
    }

    static std::string generate_tex_env_gl_apply_operand_string(const std::string source, const std::uint64_t operand, const bool is_for_rgb) {
        switch (operand) {
        case gles_texture_env_info::SOURCE_OPERAND_COLOR:
            return source + ".rgb";

        case gles_texture_env_info::SOURCE_OPERAND_ALPHA:
            if (is_for_rgb) {
                return "vec3(" + source + ".a)";
            }
            return source + ".a";

        case gles_texture_env_info::SOURCE_OPERAND_ONE_MINUS_ALPHA: {
            std::string result = "(1.0 - " + source + ".a)";
            if (is_for_rgb)
                result = "vec3(" + result + ")";
            return result;
        }

        case gles_texture_env_info::SOURCE_OPERAND_ONE_MINUS_COLOR:
            return "(vec3(1.0) - " + source + ".rgb)";

        default:
            break;
        }

        return source + ", hereIsAnotherStringSignifingThatIwasNotSupposedToReachHereButHereIAmBeingALengthyStringThatYouAreReading";
    }

    static std::string generate_tex_env_source(const std::size_t current_tex_index, const std::uint64_t source, const std::uint64_t operand, const bool is_for_rgb) {
        return generate_tex_env_gl_apply_operand_string(generate_tex_env_gl_source_string(current_tex_index, source), operand, is_for_rgb);
    }

    static std::string generate_tex_env_combine_statement(const std::string s1, const std::string s2, const std::string s3, const bool is_for_rgb, const std::uint64_t combine_func) {
        std::string dest_equals_to = (is_for_rgb ? "\toColor.rgb = " : "\toColor.a = ");
        std::string half_vec = (is_for_rgb ? "vec3(0.5)" : "0.5");
        std::string one_vec = (is_for_rgb ? "vec3(1.0)" : "1.0");
        static const char *END_STATEMENT_MARK = ";\n";
        
        switch (combine_func) {
        case gles_texture_env_info::SOURCE_COMBINE_REPLACE:
            return dest_equals_to + s1 + END_STATEMENT_MARK;

        case gles_texture_env_info::SOURCE_COMBINE_MODULATE:
            return dest_equals_to + s1 + " * " + s2 + END_STATEMENT_MARK;

        case gles_texture_env_info::SOURCE_COMBINE_ADD:
            return dest_equals_to + s1 + " + " + s2 + END_STATEMENT_MARK;

        case gles_texture_env_info::SOURCE_COMBINE_ADD_SIGNED:
            return dest_equals_to + s1 + " + " + s2 + " - " + half_vec + END_STATEMENT_MARK;

        case gles_texture_env_info::SOURCE_COMBINE_INTERPOLATE:
            return dest_equals_to + s1 + " * " + s3 + " + " + s2 + " * (" + one_vec + " - " + s3 + ")" + END_STATEMENT_MARK;

        case gles_texture_env_info::SOURCE_COMBINE_SUBTRACT:
            return dest_equals_to + s1 + " - " + s2 + END_STATEMENT_MARK;

        case gles_texture_env_info::SOURCE_COMBINE_DOT3_RGB:
        case gles_texture_env_info::SOURCE_COMBINE_DOT3_RGBA: {
            std::string result = fmt::format("tempResult = 4 * (({}.r - 0.5) * ({}.r - 0.5) + ({}.g - 0.5) * ({}.g - 0.5) + ({}.b - 0.5) * ({}.b - 0.5));\n",
                s1, s2, s1, s2, s1, s2);

            if (combine_func == gles_texture_env_info::SOURCE_COMBINE_DOT3_RGB) {
                return "\toColor.rgb = vec3(tempResult);\n";
            }

            return "\toColor.rgba = vec4(tempResult);\n";
        }

        default:
            break;
        }

        return s1 + s2 + s3 + " SOMYSTERYGUYWHYISTHISREACHINGHERE";
    }

    std::string generate_gl_fragment_shader(const std::uint64_t fragment_statuses, const std::uint32_t active_texs,
        gles_texture_env_info *tex_env_infos, const bool is_es) {
        std::string input_decl = "";
        std::string uni_decl = "";
        std::string main_body = "";
        std::string external_func = "";

        if (is_es) {
            input_decl += "#version 300 es\n"
                          "precision highp float;\n";
        } else {
            input_decl += "#version 140\n";
        }

        input_decl += "in vec4 mColor;\n"
                    "in vec3 mNormal;\n"
                    "in vec4 mMyPos;\n";

        uni_decl += "uniform vec4 uMaterialAmbient;\n"
                    "uniform vec4 uMaterialDiffuse;\n"
                    "uniform vec4 uMaterialSpecular;\n"
                    "uniform vec4 uMaterialEmission;\n"
                    "uniform float uMaterialShininess;\n"
                    "uniform vec4 uGlobalAmbient;\n";

        main_body += "void main() {\n"
                     "\toColor = mColor;\n"
                     "\tfloat tempResult = 0.0;\n";

        for (std::uint8_t i = 0; i < GLES1_EMU_MAX_CLIP_PLANE; i++) {
            if (fragment_statuses & (1 << (i + egl_context_es1::FRAGMENT_STATE_CLIP_PLANE_BIT_POS))) {
                uni_decl += fmt::format("uniform vec4 uClipPlane{};\n", i);
                main_body += fmt::format("\tfloat uClipDistResult{} = dot(mMyPos, uClipPlane{});\n"
                    "\tif (uClipDistResult{} < 0.0) discard;\n", i, i, i);
            }
        }

        if (active_texs != 0) {
            for (std::size_t i = 0; i < GLES1_EMU_MAX_TEXTURE_COUNT; i++) {
                if (active_texs & (1 << i)) {
                    uni_decl += fmt::format("uniform sampler2D uTexture{};\n", i);
                    uni_decl += fmt::format("uniform vec4 uTextureEnvColor{};\n", i);
                    input_decl += fmt::format("in vec4 mTexCoord{};\n", i);

                    if (tex_env_infos[i].env_mode_ != gles_texture_env_info::ENV_MODE_COMBINE) {
                        switch (tex_env_infos[i].env_mode_) {
                        case gles_texture_env_info::ENV_MODE_REPLACE:
                            main_body += fmt::format("\toColor = texture(uTexture{}, mTexCoord{}.xy);\n", i, i, i);
                            break;

                        case gles_texture_env_info::ENV_MODE_ADD:
                            main_body += fmt::format("\tvec4 pixelTex{} = texture(uTexture{}, mTexCoord{}.xy);\n", i, i, i);
                            main_body += fmt::format("\toColor.rgb += pixelTex{}.rgb;\n", i);
                            main_body += fmt::format("\toColor.a *= pixelTex{}.a;\n", i);
                            break;

                        case gles_texture_env_info::ENV_MODE_MODULATE:
                            main_body += fmt::format("\toColor *= texture(uTexture{}, mTexCoord{}.xy);\n", i, i);
                            break;

                        case gles_texture_env_info::ENV_MODE_BLEND:
                            main_body += fmt::format("\tvec4 pixelTex{} = texture(uTexture{}, mTexCoord{}.xy);\n", i, i, i);
                            main_body += fmt::format("\toColor.rgb = oColor.rgb * (vec3(1.0) - pixelTex{}.rgb) + uTextureEnvColor{}.rgb * pixelTex{}.rgb;\n", i, i, i);
                            main_body += fmt::format("\toColor.a *= pixelTex{}.a;\n", i);
                            break;

                        case gles_texture_env_info::ENV_MODE_DECAL:
                            main_body += fmt::format("\tvec4 pixelTex{} = texture(uTexture{}, mTexCoord{}.xy);\n", i, i, i);
                            main_body += fmt::format("\toColor.rgb = oColor.rgb * (vec3(1.0) - pixelTex{}.a) + pixelTex{}.rgb * pixelTex{}.a;\n", i, i, i);
                            main_body += fmt::format("\toColor.a = pixelTex{}.a;\n", i);
                            break;

                        default:
                            break;
                        }
                    } else {
                        // Get the pixel sample first...
                        main_body += fmt::format("\tvec4 pixelTex{} = texture(uTexture{}, mTexCoord{}.xy);\n", i, i, i);

                        main_body += generate_tex_env_combine_statement(generate_tex_env_source(i, tex_env_infos[i].src0_rgb_, tex_env_infos[i].src0_rgb_op_, true),
                            generate_tex_env_source(i, tex_env_infos[i].src1_rgb_, tex_env_infos[i].src1_rgb_op_, true),
                            generate_tex_env_source(i, tex_env_infos[i].src2_rgb_, tex_env_infos[i].src2_rgb_op_, true),
                            true, tex_env_infos[i].combine_rgb_func_);

                        main_body += generate_tex_env_combine_statement(generate_tex_env_source(i, tex_env_infos[i].src0_a_, tex_env_infos[i].src0_a_op_, false),
                            generate_tex_env_source(i, tex_env_infos[i].src1_a_, tex_env_infos[i].src1_a_op_, false),
                            generate_tex_env_source(i, tex_env_infos[i].src2_a_, tex_env_infos[i].src2_a_op_, false),
                            false, tex_env_infos[i].combine_a_func_);
                    }
                }
            }
        }


        if (fragment_statuses & egl_context_es1::FRAGMENT_STATE_LIGHTING_ENABLE) {
            // Apply lights
            uni_decl +=
                "\n"
                "struct TLightInfo {\n"
                "\tvec4 mDirOrPosition;\n"
                "\tvec4 mAmbient;\n"
                "\tvec4 mDiffuse;\n"
                "\tvec4 mSpecular;\n"
                "\tvec3 mSpotDir;\n"
                "\tfloat mSpotCutoff;\n"
                "\tfloat mSpotExponent;\n"
                "\tvec3 mAttenuation;\n"
                "};\n";

            external_func +=
                "vec4 calculateLight(TLightInfo info, vec3 normal, vec4 explicitAmbient, vec4 explicitDiffuse) {\n"
                "\tvec3 lightDir = vec3(0.0);\n"
                "\tfloat attenuation = 1.0;\n"
                "\tif (info.mDirOrPosition.w == 0.0) {\n"
                "\t\tlightDir = normalize(info.mDirOrPosition.xyz);\n"
                "\t} else {\n"
                "\t\tlightDir = normalize(info.mDirOrPosition.xyz - mMyPos.xyz);\n"
                "\t\tfloat dist = length(info.mDirOrPosition.xyz - mMyPos.xyz);\n"
                "\t\tattenuation = 1.0 / (info.mAttenuation.x + dist * info.mAttenuation.y + dist * dist * info.mAttenuation.z);\n"
                "\t}\n"
                "\tfloat diffuseFactor = max(dot(normal, lightDir), 0.0);\n"
                "\tfloat specularFactor = max(dot(normal, normalize(lightDir + vec3(0.0, 0.0, 1.0))), 0.0);\n"
                "\tif ((diffuseFactor > 0.0) && (specularFactor > 0.0))\n"
                "\t\tspecularFactor = exp(uMaterialShininess * log(specularFactor));\n"
                "\telse\n"
                "\t\tspecularFactor = 0.0;\n"
                "\tvec4 ambient = info.mAmbient * explicitAmbient;\n"
                "\tvec4 diffuse = info.mDiffuse * diffuseFactor * explicitDiffuse;\n"
                "\tvec4 specular = info.mSpecular * specularFactor * uMaterialSpecular;\n"
                "\tfloat spotAngle = dot(lightDir, normalize(info.mSpotDir));\n"
                "\tfloat spotConstant = 1.0;\n"
                "\tif ((info.mDirOrPosition.w == 0.0) || (info.mSpotCutoff == 180.0)) spotConstant = 1.0;\n"
                "\telse {\n"
                "\t\tif (spotAngle < cos(radians(info.mSpotCutoff))) spotConstant = 0.0;\n"
                "\t\telse spotConstant = pow(spotAngle, info.mSpotExponent);\n"
                "\t}\n"
                "\treturn attenuation * spotConstant * (ambient + diffuse + specular);\n"
                "}\n";

            main_body += "\tvec4 gActualMaterialAmbient;\n"
                        "\tvec4 gActualMaterialDiffuse;\n";

            if ((fragment_statuses & egl_context_es1::FRAGMENT_STATE_COLOR_MATERIAL_ENABLE) || (active_texs != 0)) {
                // Texture or previous computed color is used instead of specified material...
                main_body += "\tgActualMaterialAmbient = oColor;\n"
                            "\tgActualMaterialDiffuse = oColor;\n";
            } else {
                main_body += "\tgActualMaterialAmbient = uMaterialAmbient;\n"
                            "\tgActualMaterialDiffuse = uMaterialDiffuse;\n";
            }

            main_body += "\t// Clear color to make way for lighting\n"
                        "\toColor = vec4(0.0);\n";

            for (std::size_t i = 0, mask = egl_context_es1::FRAGMENT_STATE_LIGHT0_ON; i < GLES1_EMU_MAX_LIGHT; i++, mask <<= 1) {
                if (fragment_statuses & mask) {
                    uni_decl += fmt::format("uniform TLightInfo uLight{};\n", i);
                    main_body += fmt::format("\toColor += calculateLight(uLight{}, ", i);

                    if (fragment_statuses & egl_context_es1::FRAGMENT_STATE_LIGHT_TWO_SIDE) {
                        main_body += "gl_FrontFacing ? mNormal : -mNormal, gActualMaterialAmbient, gActualMaterialDiffuse);\n";
                    } else {
                        main_body += "mNormal, gActualMaterialAmbient, gActualMaterialDiffuse);\n";
                    }
                }
            }
        }

        // Apply fog
        if (fragment_statuses & egl_context_es1::FRAGMENT_STATE_FOG_ENABLE) {
            uni_decl += "uniform vec4 uFogColor;\n";

            // Must negate z. So the original formula got reversed a bit
            switch (fragment_statuses & egl_context_es1::FRAGMENT_STATE_FOG_MODE_MASK) {
            case egl_context_es1::FRAGMENT_STATE_FOG_MODE_LINEAR: {
                uni_decl += "uniform float uFogStart;\n";
                uni_decl += "uniform float uFogEnd;\n";

                main_body += "\tfloat fogF = (uFogEnd + mMyPos.z) / (uFogEnd - uFogStart);\n";
                break;
            }

            case egl_context_es1::FRAGMENT_STATE_FOG_MODE_EXP: {
                uni_decl += "uniform float uFogDensity;\n";
                main_body += "\tfloat fogF = exp(uFogDensity * mMyPos.z);\n";

                break;
            }

            case egl_context_es1::FRAGMENT_STATE_FOG_MODE_EXP2:
                uni_decl += "uniform float uFogDensity;\n";
                main_body += "\tfloat fogF = exp(-(uFogDensity * uFogDensity * mMyPos.z * mMyPos.z));\n";

                break;
            }

            main_body += "\tfogF = clamp(fogF, 0.0, 1.0);\n";
            main_body += "\toColor.rgb = mix(uFogColor.rgb, oColor.rgb, fogF);\n";
        }

        if (fragment_statuses & egl_context_es1::FRAGMENT_STATE_ALPHA_TEST) {
            const std::uint32_t value = (fragment_statuses & egl_context_es1::FRAGMENT_STATE_ALPHA_FUNC_MASK)
                >> egl_context_es1::FRAGMENT_STATE_ALPHA_TEST_FUNC_POS;

            uni_decl += "uniform float uAlphaTestRef;\n";

            switch (value + GL_NEVER_EMU) {
            case GL_NEVER_EMU:
                main_body += "\tdiscard;\n";
                break;

            case GL_ALWAYS_EMU:
                break;

            case GL_LESS_EMU:
                main_body += "\tif (oColor.a >= uAlphaTestRef) discard;\n";
                break;

            case GL_LEQUAL_EMU:
                main_body += "\tif (oColor.a > uAlphaTestRef) discard;\n";
                break;

            case GL_GREATER_EMU:
                main_body += "\tif (oColor.a <= uAlphaTestRef) discard;\n";
                break;

            case GL_GEQUAL_EMU:
                main_body += "\tif (oColor.a < uAlphaTestRef) discard;\n";
                break;

            case GL_EQUAL_EMU:
                main_body += "\tif (oColor.a != uAlphaTestRef) discard;\n";
                break;

            case GL_NOTEQUAL_EMU:
                main_body += "\tif (oColor.a == uAlphaTestRef) discard;\n";
                break;

            default:
                break;
            }
        }

        main_body += "}";
        return input_decl + uni_decl + "out vec4 oColor;\n" + external_func + main_body;
    }
}