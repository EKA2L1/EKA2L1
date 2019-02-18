/*
 * ecom.h
 *
 *  Created on: Feb 18, 2019
 *      Author: fewds
 */

#ifndef ECOM_H_
#define ECOM_H_

/**
 * \brief Get all OpenFont rasterizer plugin through ECom.
 * 
 * This test case assumes that no other plugins are installed on other drive, which means this
 * only count plugins on ROM.
 */
void EComGetFontRasterizerPluginInfosL();

/**
 * \brief Get all OpenFont TrueType plugin through ECom.
 * 
 * This test case assumes that no other plugins are installed on other drive, which means this
 * only count plugins on ROM.
 */
void EComGetFontTrueTypePluginInfosL();

void AddEComTestCasesL();

#endif /* ECOM_H_ */
