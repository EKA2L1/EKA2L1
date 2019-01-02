/*
 * file.h
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#ifndef IO_FILE_H_
#define IO_FILE_H_

// Checking position after reading a file without position specified and with position specified
void PosAfterCustomReadL();

// Do common seekings
// This includes:
// - Seek ROM file
// - Seek start, seek end, seek current
// - Backward seek
void CommonSeekingRomL();

void AddFileTestCasesL();

#endif
