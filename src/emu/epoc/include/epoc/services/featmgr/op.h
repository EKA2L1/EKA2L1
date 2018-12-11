/*
 * Copyright (c) 2018 EKA2L1 Team / 2009 Nokia
 * 
 * This file is part of EKA2L1 project / Symbian Open Source Project
 * (see bentokun.github.com/EKA2L1).
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

enum TFeatMgrServRequest {
    /**
	* Fetches information whether a certain feature is supported.
	*
	* 0: TFeatureEntry - Feature entry
  	* 1: TInt - ret - Returns feature support value (0,1) or error code.
  	*/
    EFeatMgrFeatureSupported = 0,

    /**
	* Fetches information of subset of features support-status.
	*
	* 0: TInt - Count of features, notification will be requested
	* 1: TPtr8 - Feature UID array (RFeatureUidArray)
  	* 2: TInt - Count of features in response.
  	*/
    EFeatMgrFeaturesSupported,

    /**
	* Lists all supported features.
	*
	* 0: In return as a parameter received reference to the client owned 
    *    RFeatureUidArray array contains Ids of all supported features.
  	*/
    EFeatMgrListSupportedFeatures,

    /**
	* Number of all supported features.
	*
	* 0: TInt - err - Returns the number of all supported features.
  	*/
    EFeatMgrNumberOfSupportedFeatures,

    /**
	* Request for single or subset of features notification.
	*
	* 0: TPtr8 - On change notification, server sets changed UID (TUid)
  	* 1: TRequestStatus - Request to be signaled.
  	*/
    EFeatMgrReqNotify,

    /**
	* Uids associated with request for single or subset of features notification.
	*
	* 0: TInt - Count of features, notification will be requested
	* 1: TPtr8 - Feature UID array (RFeatureUidArray)
  	* 2: TInt - err - Operation error code.
  	*/
    EFeatMgrReqNotifyUids,

    /**
	* Request cancellation of single feature notification.
	*
	* 0: TUid - Feature UID
  	* 1: TRequestStatus - Request to be canceled
  	* 2: TInt - err - Operation error code.
  	*/
    EFeatMgrReqNotifyCancel,

    /**
	* Request cancellation of all features notification.
	*
  	* 0: TInt - err - Operation error code.
  	*/
    EFeatMgrReqNotifyCancelAll,

#ifdef EXTENDED_FEATURE_MANAGER_TEST
    /**
  	* Number of notify features.
  	*
   	*/
    EFeatMgrNumberOfNotifyFeatures,

    /**
   	* Number of allocated cells.
   	*
   	*/
    EFeatMgrCountAllocCells,
#endif

    /**
	* Enables a certain feature.
	*
	* 0: TUid - Feature UID
  	* 1: TInt - err - Operation error code.
  	*/
    EFeatMgrEnableFeature,

    /**
	* Disables a certain feature.
	*
	* 0: TUid - Feature UID
  	* 1: TInt - err - Operation error code.
  	*/
    EFeatMgrDisableFeature,

    /**
	* Adds a feature entry.
	*
	* 0: TFeatureEntry - Feature entry
  	* 1: TInt - err - Operation error code.
  	*/
    EFeatMgrAddFeature,

    /**
	* Sets a certain feature and associated data.
	*
	* 0: TUid - Feature UID
  	* 1: TBool - Feature enable or disable.
  	* 2: TInt - Feature data.
  	* 3: TInt - err - Operation error code.
  	*/
    EFeatMgrSetFeatureAndData,

    /**
	* Sets certain feature's data.
	*
	* 0: TUid - Feature UID
  	* 1: TInt - Feature data.
  	* 2: TInt - err - Operation error code.
  	*/
    EFeatMgrSetFeatureData,

    /**
	* Deletes a feature entry.
	*
	* 0: TUid - Feature UID
  	* 1: TInt - err - Operation error code.
  	*/
    EFeatMgrDeleteFeature,

    /**
    * Software Installation started
    * 
    */
    EFeatMgrSWIStart,

    /**
    * Software Installation ended
    * 
    */
    EFeatMgrSWIEnd,

    EFeatMgrResourceMark,
    EFeatMgrResourceCheck,
    EFeatMgrResourceCount,
    EFeatMgrSetHeapFailure

};
