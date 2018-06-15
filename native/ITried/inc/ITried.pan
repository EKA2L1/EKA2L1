/*
 ============================================================================
 Name		: ITried.pan
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : This file contains panic codes.
 ============================================================================
 */

#ifndef __ITRIED_PAN__
#define __ITRIED_PAN__

/** ITried application panic codes */
enum TITriedPanics
	{
	EITriedUi = 1
	// add further panics here
	};

inline void Panic(TITriedPanics aReason)
	{
	_LIT(applicationName, "ITried");
	User::Panic(applicationName, aReason);
	}

#endif // __ITRIED_PAN__
