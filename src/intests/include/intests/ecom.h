/*
 * ecom.h
 *
 *  Created on: Feb 18, 2019
 *      Author: fewds
 */

#ifndef ECOM_H_
#define ECOM_H_

/**
 * \brief Get all OpenFont rasterizer plugins through ECom.
 * 
 * This test case assumes that no other plugins are installed on other drive, which means this
 * only count plugins on ROM.
 */
void EComGetFontRasterizerPluginInfosL();

/**
 * \brief Get all OpenFont TrueType plugins through ECom.
 * 
 * This test case assumes that no other plugins are installed on other drive, which means this
 * only count plugins on ROM.
 */
void EComGetFontTrueTypePluginInfosL();

/**
 * \brief Get all CDL plugins through ECom.
 * 
 * This test case assumes that no other plugins are installed on other drive, which means this
 * only count plugins on ROM.
 */
void EComGetCdlPluginInfosL();

void AddEComTestCasesL();

#endif /* ECOM_H_ */
