/*
 * gnuVG - a free Vector Graphics library
 * Copyright (C) 2016 by Anton Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//--------------------------------------------------------------------------------------
// Copyright 2014 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//
// Created by: filip.strugar@intel.com
//--------------------------------------------------------------------------------------
//
// - function "GenerateGaussShaderKernelWeightsAndOffsets" will generate shader
//      constants for high performance Gaussian blur filter implementation (separable,
//      using hardware linear filter when sampling to get two samples at a time)
//
// - function "GenerateGaussFunctionCode" will generate GLSL code using the above
//      constants; for HLSL replace vec2/vec3 with float2/float3, etc, "should work"
//
// - Acceptable kernel sizes are of 4*n-1 (3, 7, 11, 15, ...)
//
// - Gauss distribution Sigma value is generated "ad hoc": feel free to modify the code
//      for your purpose. Performance of the algorithm is only dependent on kernel size.
//


#pragma once

#include <assert.h>
#include <vector>
#include <string>
#include <stdarg.h>
#include <math.h>

#include <dispatch/libraries/vg/gnuVG_error.hh>

namespace gnuVG {

	inline std::string stringFormatA( const char * fmt, ... ) {
		char buff[4096];
		va_list args;
		va_start(args, fmt);
		(void) vsnprintf( buff, sizeof(buff) - 1, fmt, args); // C4996
		return std::string( buff );
	}

	inline std::vector<double> GenerateSeparableGaussKernel( double sigma, int kernelSize ) {
		int halfKernelSize = kernelSize/2;

		std::vector<double> kernel;
		kernel.resize( kernelSize );

		const double cPI= 3.14159265358979323846;
		double mean     = halfKernelSize;
		double sum      = 0.0;
		for (int x = 0; x < kernelSize; ++x) {
			kernel[x] = (float)sqrt( exp( -0.5 * (pow((x-mean)/sigma, 2.0) + pow((mean)/sigma,2.0)) )
						 / (2 * cPI * sigma * sigma) );
			sum += kernel[x];
		}
		for (int x = 0; x < kernelSize; ++x)
			kernel[x] /= (float)sum;

		return kernel;
	}

	inline std::vector<float> GetAppropriateSeparableGauss( int kernelSize ) {
		// Search for sigma to cover the whole kernel size with sensible values (might not be ideal for all cases quality-wise but is good enough for performance testing)
		const double epsilon = 2e-2f / kernelSize;
		double searchStep = 1.0;
		double sigma = 1.0;
		while( true ) {

			std::vector<double> kernelAttempt = GenerateSeparableGaussKernel( sigma, kernelSize );
			if( kernelAttempt[0] > epsilon ) {
				if( searchStep > 0.02 ) {
					sigma -= searchStep;
					searchStep *= 0.1;
					sigma += searchStep;
					continue;
				}
				std::vector<float> retVal;
				for (int i = 0; i < kernelSize; i++)
					retVal.push_back( (float)kernelAttempt[i] );
				return retVal;
			}

			sigma += searchStep;

			if( sigma > 1000.0 ) {
				GNUVG_ERROR("gnuVG_gaussianblur: sigma > 1000.0\n");
				assert( false ); // not tested, preventing infinite loop
			}
		}

		return std::vector<float>();
	}

	inline std::string GenerateGaussShaderKernelWeightsAndOffsets( int kernelSize, bool forPreprocessorDefine = false, bool workaroundForNoCLikeArrayInitialization = false ) {
		// Gauss filter kernel & offset creation
		std::vector<float> inputKernel = GetAppropriateSeparableGauss(kernelSize);

		std::vector<float> oneSideInputs;
		for( int i = (kernelSize/2); i >= 0; i-- ) {
			if( i == (kernelSize/2) )
				oneSideInputs.push_back( (float)inputKernel[i] * 0.5f );
			else
				oneSideInputs.push_back( (float)inputKernel[i] );
		}

		int numSamples = oneSideInputs.size()/2;

		std::vector<float> weights;

		for( int i = 0; i < numSamples; i++ ) {
			float sum = oneSideInputs[i*2+0] + oneSideInputs[i*2+1];
			weights.push_back(sum);
		}

		std::vector<float> offsets;

		for( int i = 0; i < numSamples; i++ ) {
			offsets.push_back( i*2.0f + oneSideInputs[i*2+1] / weights[i] );
		}

		std::string indent = "    ";

		std::stringstream fshad;

		std::string eol = "\n";
		fshad << indent + "//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////;" << eol;
		fshad << indent << "// Kernel diameter " << kernelSize << eol;
		fshad << indent << "//" << eol;
		fshad << indent << "const int stepCount = " << numSamples << ";" << eol;

		if( !workaroundForNoCLikeArrayInitialization ) {
			if( !forPreprocessorDefine) fshad << indent << "//" << eol;
			fshad << indent << "const float gWeights[stepCount] ={" << eol;
			for( int i = 0; i < numSamples; i++ )
				fshad << indent << "   " << weights[i]
				      << ((i!=(numSamples-1))?(","):("")) << eol;
			fshad << indent << "};" << eol
			      << indent << "const float gOffsets[stepCount] ={" << eol;
			for( int i = 0; i < numSamples; i++ )
				fshad << indent << "   " << offsets[i]
				      << ((i!=(numSamples-1))?(","):("")) << eol;
			fshad << indent << "};" << eol;
		} else {
			if( !forPreprocessorDefine) fshad << indent << "//" << eol;

			fshad << indent << "float gWeights[stepCount];" << eol;

			for( int i = 0; i < numSamples; i++ )
				fshad << indent << " gWeights[" << i << "] = " << weights[i] << ";" << eol;
			fshad << eol;
			fshad << indent << "float gOffsets[stepCount];" << eol;

			for( int i = 0; i < numSamples; i++ )
				fshad << indent << " gOffsets[" << i << "] = " << offsets[i] << ";" << eol;
			fshad << eol;
		}

		fshad << indent << "//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////;" << eol;

		return fshad.str();
	}

	inline std::string GenerateGaussFunctionCode( int kernelSize, bool workaroundForNoCLikeArrayInitialization = false ) {
		std::string shaderCode;

		// force odd size
		kernelSize |= 0x1;

		std::string eol = "\n";

		shaderCode += eol;
		shaderCode += "// automatically generated by GenerateGaussFunctionCode in gnuVG_gaussianblur.hh                                                                                            " + eol;
		shaderCode += "vec4 GaussianBlur( sampler2D tex0, vec2 centreUV, vec2 halfPixelOffset, vec2 pixelOffset )                                                                           " + eol;
		shaderCode += "{                                                                                                                                                                    " + eol;
		shaderCode += "    vec4 colOut = vec4( 0, 0, 0, 0 );                                                                                                                                   " + eol;
		shaderCode += "                                                                                                                                                                     " + eol;
		shaderCode += GenerateGaussShaderKernelWeightsAndOffsets( kernelSize, false, workaroundForNoCLikeArrayInitialization );
		shaderCode += "                                                                                                                                                                     " + eol;
		shaderCode += "    for( int i = 0; i < stepCount; i++ )                                                                                                                             " + eol;
		shaderCode += "    {                                                                                                                                                                " + eol;
		shaderCode += "        vec2 texCoordOffset = gOffsets[i] * pixelOffset;                                                                                                           " + eol;
		shaderCode += "        vec4 c1 = texture( tex0, centreUV + texCoordOffset );" + eol;
		shaderCode += "        vec4 c2 = texture( tex0, centreUV - texCoordOffset );" + eol;
		shaderCode += "        vec4 col = c1 + c2;" + eol;
		shaderCode += "        colOut += gWeights[i] * col;                                                                                                                               " + eol;
		shaderCode += "    }                                                                                                                                                                " + eol;
		shaderCode += "                                                                                                                                                                     " + eol;
		shaderCode += "    return colOut;                                                                                                                                                   " + eol;
		shaderCode += "}                                                                                                                                                                    " + eol;
		shaderCode += eol;

		return shaderCode;
	}

};
