/*	Oscar, a hardware abstraction framework for the LeanXcam and IndXcam.
	Copyright (C) 2008 Supercomputing Systems AG
	
	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.
	
	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
	General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*! @file
 * @brief Configuration file module implementation for target and host
 * 
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "oscar.h"

struct OscModule OscModule_cfg = {
	.name = "cfg",
	.dependencies = {
		&OscModule_log,
		NULL // To end the flexible array.
	}
};

/*! @brief Macro defining the maximal number of open configuration files */
#define CONFIG_FILE_MAX_NUM 5
/*! @brief Macro defining the maximal file name string size */
#define CONFIG_FILE_NAME_MAX_SIZE CONFIG_VAL_MAX_SIZE
/*! @brief Macro defining the section suffix string */
#define CONFIG_FILE_SECTION_SUFFIX "\n"
/*! @brief Macro defining the tag suffix string */
#define CONFIG_FILE_TAG_SUFFIX ": "
/*! @brief Macro defining the tag suffix string */
#define CONFIG_FILE_LABEL_PREFIX "\n"
/*! @brief Macro defining the escape characters for the string scanning */
#define CONFIG_FILE_ESCAPE_CHARS "%1024[^\n]"

/*! @brief Structure containing the file content */
struct CFG_FILE_CONTENT {
	char *data; /* +1 to add string termination \0 */
	unsigned int dataSize; /* allocated memory of data array */
	char fileName[CONFIG_FILE_NAME_MAX_SIZE];
};

/*! @brief Config File Content handels. */
struct OSC_CFG {
	uint16 nrOfContents;        /*!< @brief Number of managed contents */
	struct CFG_FILE_CONTENT contents[CONFIG_FILE_MAX_NUM];
};

/*======================= Private methods ==============================*/

/*********************************************************************//*!
 * @brief Helper function to write the file content
 * 
 * @param hFileContent Handle to the File content.
 * @param all True if the whole buffer shall be written.
 * @return SUCCESS or an appropriate error code otherwise
 *//*********************************************************************/
OSC_ERR OscCfgFlushContentHelper(const CFG_FILE_CONTENT_HANDLE hFileContent, bool all);

/*********************************************************************//*!
 * @brief Helper function to find the beginning of the value string in the file
 * 
 * @param contentIndex Index to the File content.
 * @param pKey The name of the section and tag.
 * @param pPStrVal Return pointer to the value in the file.
 * @return SUCCESS or an appropriate error code otherwise
 *//*********************************************************************/
OSC_ERR OscCfgGetValPtr(
		const unsigned int  contentIndex,
		const struct CFG_KEY *pKey,
		char **pPStrVal);

/*********************************************************************//*!
 * @brief Finds subString in String
 * 
 * @param subString Pointer to substring.
 * @param subStringLen Substring length.
 * @param string Pointer to string.
 * @return Pointer to char in string after subString, NULL if no subString is found
 *//*********************************************************************/
char* OscCfgIsSubStr(
		const char *subString,
		const size_t subStringLen,
		const char *string);

/*********************************************************************//*!
 * @brief Finds label at the beginning of the line of a text.
 * 
 * @param label Pointer to label. If NULL, beginning of text is returned
 * @param labelSuffix Pointer to label suffix (e.g. ": " for tags).
 * @param text Pointer to text.
 * @return Pointer to char in test after label suffix, NULL if label not found
 *//*********************************************************************/
char* OscCfgFindNewlineLabel(
		const char* label,
		const char* labelSuffix,
		char* text);

/*********************************************************************//*!
 * @brief Replaces oldStr with newStr in text
 * 
 * @param contentIndex Index to content structure.
 * @param oldStr Pointer to the old string, which will be replaced.
 * @param newStr Pointer to new string.
 * @param text Pointer to text.
 * @return SUCCESS or an appropriate error code otherwise
 *//*********************************************************************/
OSC_ERR OscCfgReplaceStr(
		const unsigned int  contentIndex,
		const char *oldStr,
		const char *newStr,
		char* text);


/*********************************************************************//*!
 * @brief Appends label to the file content
 * 
 * @param text Pointer to text.
 * @param maxTextLen maxFileSize + 1.
 * @param label Pointer to label. If NULL, nothing is appended
 * @param labelPrefix Pointer to labelPrefix.
 * @param labelSuffix Pointer to labelSuffix.
 * @return pointer to char after labelSuffix
 *//*********************************************************************/
char* OscCfgAppendLabel(
		char* text,
		const unsigned int maxTextLen,
		const char* label,
		const char* labelPrefix,
		const char* labelSuffix);

/*********************************************************************//*!
 * @brief Helper to find the first occurence of an invalid character
 * 
 * @param str Character array to search in.
 * @param strSize Array length of str.
 * @return index of first invalid char.
 *//*********************************************************************/
unsigned int OscCfgFindInvalidChar(const unsigned char *str, const unsigned int strSize);

/*!
	@brief Get the value of a U-Boot environment variable.
	@param key The name of the variable.
	@param value Is set to point to the value in a static buffer.
*/
OscFunctionDeclare(static getUBootEnv, char * key, char ** value)

/*!
	@brief Parse an integer from a string.
	@param str String containing the integer.
	@param res Is set to the number parsed from the string.
*/
OscFunctionDeclare(static parseInteger, char * str, int * res)

/*!
	@brief Get the version of the running uClinux version.
	@param res Is set to point to the value in a static buffer.
*/
OscFunctionDeclare(static getUClinuxVersion, char ** res)

/*! @brief The module singelton instance. */
struct OSC_CFG cfg;

OSC_ERR OscCfgRegisterFile(
		CFG_FILE_CONTENT_HANDLE *pFileContentHandle,
		const char *strFileName,
		const unsigned int maxFileSize)
{
	FILE    *pCfgFile;
	size_t  fileSize;
	unsigned int    actIndex;
	unsigned int    invalidCharIndex;
	
	/* find an unused file index */
	if(cfg.nrOfContents==0) {
		for(uint16 i=0; i<CONFIG_FILE_MAX_NUM; ++i) 
			cfg.contents[i].data=NULL;
		actIndex=0;
	} else {
		actIndex=0;
		while(actIndex<CONFIG_FILE_MAX_NUM && cfg.contents[actIndex].data)
			++actIndex;
	}
	
	/* check preconditions */
	if(pFileContentHandle == NULL || strFileName == NULL || strFileName[0] == '\0')
	{
		OscLog(ERROR, "%s(0x%x, %s): Invalid parameter.\n",
				__func__, pFileContentHandle, strFileName);
		return -ECFG_INVALID_FUNC_PARAMETER;
	}
	if(cfg.nrOfContents >= CONFIG_FILE_MAX_NUM || actIndex >= CONFIG_FILE_MAX_NUM) {
		OscLog(ERROR, "%s: too many handles open (%d=%d) !\n",
				__func__, cfg.nrOfContents, CONFIG_FILE_MAX_NUM);
		return -ECFG_NO_HANDLES;
	}

	/* copy file name and open file */
	pCfgFile = fopen(strFileName, "r");
	if(pCfgFile == NULL)
	{
		OscLog(WARN, "%s: Unable to open config file %s!\n",
				__func__, strFileName);
		return -ECFG_UNABLE_TO_OPEN_FILE;
	}

	/* save data in content manager */
	cfg.contents[actIndex].data = malloc(maxFileSize + 1);
	if (cfg.contents[actIndex].data == NULL)
	{
		OscLog(ERROR, "%s: could not allocate memory!\n",
				__func__);
		return -ECFG_ERROR;
	}
	fileSize = fread(cfg.contents[actIndex].data, sizeof(char), maxFileSize + 1, pCfgFile);
	fclose(pCfgFile);
	if (fileSize == maxFileSize + 1)
	{
		OscLog(ERROR, "%s: config file too long!\n",
				__func__);
		free(cfg.contents[actIndex].data);
		cfg.contents[actIndex].data=NULL;
		return -ECFG_UNABLE_TO_OPEN_FILE;
	}
	cfg.nrOfContents++;
	
	/* append string termination */
	invalidCharIndex = OscCfgFindInvalidChar((unsigned char *)cfg.contents[actIndex].data, fileSize);
	cfg.contents[actIndex].data[invalidCharIndex] = '\0';
	OscLog(DEBUG, "%s: string length set to %d\n",
			__func__, invalidCharIndex);

	cfg.contents[actIndex].dataSize = maxFileSize + 1;
	*pFileContentHandle = actIndex+1; /* return content handle */
	strcpy(cfg.contents[actIndex].fileName, strFileName); /* store file name */
	
/*    OscLog(DEBUG, "Read config file (%s):\n%s\n", strFileName, cfg.contents[actIndex].data); does not work on OSC*/

	return SUCCESS;
}

OSC_ERR OscCfgUnregisterFile(CFG_FILE_CONTENT_HANDLE pFileContentHandle) {
	
	if(pFileContentHandle<=0 || pFileContentHandle>CONFIG_FILE_MAX_NUM
			|| cfg.contents[pFileContentHandle-1].data==NULL) {
		return(EINVALID_PARAMETER);
	}
	
	free(cfg.contents[pFileContentHandle-1].data);
	cfg.contents[pFileContentHandle-1].data=NULL;
	
	--cfg.nrOfContents;
	
	return(SUCCESS);
}

OSC_ERR OscCfgDeleteAll( void)
{
	return SUCCESS;
}

OSC_ERR OscCfgFlushContent(const CFG_FILE_CONTENT_HANDLE hFileContent)
{
	return OscCfgFlushContentHelper(hFileContent, FALSE);
}

OSC_ERR OscCfgFlushContentAll(const CFG_FILE_CONTENT_HANDLE hFileContent)
{
	return OscCfgFlushContentHelper(hFileContent, TRUE);
}

OSC_ERR OscCfgGetStr(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		struct CFG_VAL_STR *pVal)
{
	char *pStrVal = NULL;
	int  stdErr;
	OSC_ERR err;
	
	/* check preconditions */
	if(pKey == NULL || pVal == NULL || hFileContent == 0 || hFileContent > CONFIG_FILE_MAX_NUM)
	{
		OscLog(ERROR, "%s(%d, 0x%x, 0x%x): Invalid parameter.\n",
				__func__, hFileContent, pKey, pVal);
		return -ECFG_INVALID_FUNC_PARAMETER;
	}

	/* find value pointer */
	err = OscCfgGetValPtr(hFileContent-1, pKey, &pStrVal);
	if (err != SUCCESS)
	{
		return err;
	}
	/* function may return null pointer */
	else if(pStrVal == NULL)
	{
		/*OscLog(WARN, "%s: tag or section not found (%s)!\n",
				__func__, pKey->strTag);*/
		return -ECFG_INVALID_KEY;
	}
	
	/* scan value at beginning of file */
	stdErr = sscanf(pStrVal, CONFIG_FILE_ESCAPE_CHARS, pVal->str);
	if (stdErr == EOF)
	{
		OscLog(WARN, "%s: no val found! (TAG=%s)\n",
				__func__, pKey->strTag);
		return -ECFG_INVALID_VAL;
	}

	OscLog(DEBUG, "Read Tag '%s': Value '%s'\n", pKey->strTag, pVal->str);
	return SUCCESS;
}

OSC_ERR OscCfgGetStrRange(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		struct CFG_VAL_STR *pVal,
		const uint32 len,
		const char* pDefault)
{
	OSC_ERR err;
	err = OscCfgGetStr( hFileContent, pKey, pVal);
	
    if( (err == SUCCESS) && (strlen(pVal->str) > len) && (len != -1))
    {
    	err = ECFG_INVALID_RANGE;
	}  
	if( err != SUCCESS && pDefault != NULL)
	{
    	strcpy( pVal->str, pDefault);
    	err = ECFG_USED_DEFAULT;
	}	
	return err;
}

OSC_ERR OscCfgSetStr(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		const char *strNewVal)
{
	char *pStrSecStart = NULL;  /* points to beginning of section content */
	char *pStrVal = NULL;
	struct CFG_VAL_STR oldVal;
	int  stdErr;
	OSC_ERR err;
	unsigned int index;
	
	/* check preconditions */
	if(pKey == NULL || hFileContent == 0 || hFileContent > CONFIG_FILE_MAX_NUM)
	{
		OscLog(ERROR, "%s(%d, 0x%x, 0x%x): Invalid parameter.\n",
				__func__, hFileContent, pKey, strNewVal);
		return -ECFG_INVALID_FUNC_PARAMETER;
	}
	index = hFileContent - 1;

	/* find value pointer */
	err = OscCfgGetValPtr(index, pKey, &pStrVal);
	if (err != SUCCESS)
	{
		return err;
	}
	if (pStrVal == NULL) /* if section or tag not found */
	{
		/* find section */
		pStrSecStart = OscCfgFindNewlineLabel(pKey->strSection, CONFIG_FILE_SECTION_SUFFIX, cfg.contents[index].data);
		if(pStrSecStart == NULL)
		{
			/* append section label and get pointer to content string termination */
			pStrSecStart = OscCfgAppendLabel(cfg.contents[index].data, cfg.contents[index].dataSize, pKey->strSection, CONFIG_FILE_LABEL_PREFIX, ""/* \n added with tag*/);
			if (pStrSecStart == NULL)
			{
				return -ECFG_ERROR;
			}
		}
		pStrVal = OscCfgFindNewlineLabel(pKey->strTag, CONFIG_FILE_TAG_SUFFIX, pStrSecStart);
		if(pStrVal == NULL)
		{
			/* append tag label and get pointer to content string termination */
			pStrVal = OscCfgAppendLabel(cfg.contents[index].data, cfg.contents[index].dataSize, pKey->strTag, CONFIG_FILE_LABEL_PREFIX, CONFIG_FILE_TAG_SUFFIX);
			if (pStrVal == NULL)
			{
				return -ECFG_ERROR;
			}
		}
	}
	/* scan value after tag */
	stdErr = sscanf(pStrVal, CONFIG_FILE_ESCAPE_CHARS, oldVal.str);
	if (stdErr == EOF)
	{
		oldVal.str[0] = '\0'; /* set string termination */
	}
	
	/* insert the new string into config file */
	err = OscCfgReplaceStr(index, oldVal.str, strNewVal, pStrVal);
	if (err == SUCCESS)
	{
		OscLog(DEBUG, "Wrote Tag '%s': Value '%s'\n", pKey->strTag, strNewVal);
	}
	else
	{
		OscLog(WARN, "Unable to write Tag '%s': Value '%s'\n", pKey->strTag, strNewVal);
	}
	return err;
}

OSC_ERR OscCfgSetBool(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		const bool val)
{
	OSC_ERR err;
	if(val)
	{
		err = OscCfgSetStr( hFileContent, pKey, "TRUE");
	}
	else
	{
		err = OscCfgSetStr( hFileContent, pKey, "FALSE");
	}		
	return err;
}		

OSC_ERR OscCfgSetInt(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		const int val)
{
	char strVal[ CONFIG_VAL_MAX_SIZE];
	
	sprintf( strVal, "%d", val);
	return OscCfgSetStr( hFileContent, pKey, strVal);
}		

OSC_ERR OscCfgGetInt(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		int16 *iVal)
{
	OSC_ERR err;
	int32 tmpVal;
	err = OscCfgGetInt32(hFileContent, pKey, &tmpVal);
	if (err == SUCCESS)
	{
			*iVal = (int16)tmpVal;
	}
	return err;
}

OSC_ERR OscCfgGetUInt8(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		uint8 *iVal)
{
	OSC_ERR err;
	uint32 tmpVal;
	err = OscCfgGetUInt32(hFileContent, pKey, &tmpVal);
	if (err == SUCCESS)
	{
			*iVal = (uint8)tmpVal;
	}
	return err;
}


OSC_ERR OscCfgGetIntRange(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		int16 *iVal,
		const int16 min,
		const int16 max,
		const int16 def)
{
	OSC_ERR err;
	int32 tmpVal;
	err = OscCfgGetInt32Range(hFileContent, pKey, &tmpVal, min, max, (int32)def);
	*iVal = (int16)tmpVal;
	return err;
}

OSC_ERR OscCfgGetInt8Range(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		int8 *iVal,
		const int8 min,
		const int8 max,
		const int8 def)
{
	OSC_ERR err;
	int32 tmpVal;
	err = OscCfgGetInt32Range(hFileContent, pKey, &tmpVal, min, max, (int32)def);
	*iVal = (int8)tmpVal;
	return err;
}

OSC_ERR OscCfgGetUInt8Range(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		uint8 *iVal,
		const uint8 min,
		const uint8 max,
		const uint8 def)
{
	OSC_ERR err;
	uint32 tmpVal;
	err = OscCfgGetUInt32Range(hFileContent, pKey, &tmpVal, min, max, (uint32)def);
	*iVal = (uint8)tmpVal;
	return err;
}

OSC_ERR OscCfgGetInt16Range(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		int16 *iVal,
		const int16 min,
		const int16 max,
		const int16 def)
{
	OSC_ERR err;
	int32 tmpVal;
	err = OscCfgGetInt32Range(hFileContent, pKey, &tmpVal, min, max, (int32)def);
	*iVal = (int16)tmpVal;
	return err;
}

OSC_ERR OscCfgGetUInt16Range(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		uint16 *iVal,
		const uint16 min,
		const uint16 max,
		const uint16 def)
{
	OSC_ERR err;
	uint32 tmpVal;
	err = OscCfgGetUInt32Range(hFileContent, pKey, &tmpVal, (uint32)min, (uint32)max, (uint32)def);
	*iVal = (uint16)tmpVal;
	return err;
}


OSC_ERR OscCfgGetInt32(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		int32 *iVal)
{
	struct CFG_VAL_STR val;
	OSC_ERR err;
	/* check preconditions */
	if(pKey == NULL || iVal == NULL)
	{
		OscLog(ERROR, "%s(%d, 0x%x, 0x%x): Invalid parameter.\n",
				__func__, hFileContent, pKey, iVal);
		return -ECFG_INVALID_FUNC_PARAMETER;
	}

	err = OscCfgGetStr(hFileContent, pKey, &val);
	if (err == SUCCESS)
	{
		*iVal = (int32)atoi(val.str);
	}
	return err;
}

OSC_ERR OscCfgGetUInt32(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		uint32 *iVal)
{
	struct CFG_VAL_STR val;
	OSC_ERR err;
	/* check preconditions */
	if(pKey == NULL || iVal == NULL)
	{
		OscLog(ERROR, "%s(%d, 0x%x, 0x%x): Invalid parameter.\n",
				__func__, hFileContent, pKey, iVal);
		return -ECFG_INVALID_FUNC_PARAMETER;
	}

	err = OscCfgGetStr(hFileContent, pKey, &val);
	if (err == SUCCESS)
	{
		*iVal = (uint32)atoi(val.str);
	}
	return err;
}

OSC_ERR OscCfgGetInt32Range(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		int32 *iVal,
		const int32 min,
		const int32 max,
		const int32 def)
{
	OSC_ERR err;
	err = OscCfgGetInt32(hFileContent, pKey, iVal);
	if ((max > min) && (err == SUCCESS))
	{
		if( ((*iVal < min) || (*iVal > max)) && (max!=-1) )
		{
			OscLog(WARN, "%s: Value out of range (%s: %d)!\n",
					__func__, pKey->strTag, *iVal);
			err = -ECFG_INVALID_VAL;
		}
	}
	if( err != SUCCESS)
	{
		*iVal = def;
		err = ECFG_USED_DEFAULT;
	}	
	return err;
}

OSC_ERR OscCfgGetUInt32Range(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		uint32 *iVal,
		const uint32 min,
		const uint32 max,
		const uint32 def)
{
	OSC_ERR err;
	err = OscCfgGetUInt32(hFileContent, pKey, iVal);
	if ((max > min) && (err == SUCCESS))
	{
		if( ((*iVal < min) || (*iVal > max)) && (max!=-1) )
		{
			OscLog(WARN, "%s: Value out of range (%s: %d)!\n",
					__func__, pKey->strTag, *iVal);
			err = -ECFG_INVALID_VAL;
		}
	}
	if( err != SUCCESS)
	{
		*iVal = def;
    	err = ECFG_USED_DEFAULT;		
	}
	return err;
}

OscFunction( OscCfgGetFloatRange,
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		float *iVal,
		const float min,
		const float max,
		const float def)

	struct CFG_VAL_STR val;
	float valF;
	int ret;
	OSC_ERR err;

	if( !(max!=max) && !(min!=min)) /* NAN test */
	{
		OscAssert_es( max > min, -ECFG_INVALID_VAL);
	}
	err = OscCfgGetStr( hFileContent, pKey, &val);
	if( err != SUCCESS)
	{
		*iVal = def;
		return ECFG_USED_DEFAULT;
	}
	ret = sscanf(val.str, "%f", &valF);
	if( ret == EOF)
	{
		*iVal = def;
		return ECFG_USED_DEFAULT;
	}
	if( ((min!=NAN) && (valF < min)) || ((max!=NAN) && (valF > max)) )
	{
		OscLog(WARN, "%s: Value out of range (%s: %f)!\n",
				__func__, pKey->strTag, valF);
		*iVal = def;
		return ECFG_USED_DEFAULT;
	}

	*iVal = valF;
OscFunctionEnd()

OSC_ERR OscCfgGetBool(
		const CFG_FILE_CONTENT_HANDLE hFileContent,
		const struct CFG_KEY *pKey,
		bool *iVal,
		const bool def)
{
	OSC_ERR err;
	struct CFG_VAL_STR val;
	
	err = OscCfgGetStr( hFileContent, pKey, &val);
	if( err != SUCCESS)
	{
		strcpy( val.str, "0"); // if default
	}
	
	if( (0 == strcasecmp( val.str, "TRUE"))  || 
		(0 == strcmp( val.str, "1")) )
	{	
		*iVal = TRUE;
	}
	else
	{
		if( (0 == strcasecmp( val.str, "FALSE")) || 
			(0 == strcmp( val.str, "0")) )
		{	
			*iVal = FALSE;
		}		
		else
		{
			err = -ECFG_INVALID_VAL;
		}
	}  
	if( err != SUCCESS)
	{
		*iVal = def;
    	err = ECFG_USED_DEFAULT;		
	}
	return err;		
}

#ifdef TARGET_TYPE_MESA_SR4K
#error OscCfgGetSystemInfo needs to be updated to work on the Mesa SwissRanger.
#endif

OscFunction(OscCfgGetSystemInfo, struct OscSystemInfo ** ppInfo)
	OscFunction(staticStore, char * str, char ** staticStr)
		static char buffer[1024];
		static char * pNext = buffer;
		static int remaining = sizeof buffer;
		int len = strlen(str) + 1;
		
		OscAssert_m(len < remaining, "No buffer space left.");
		
		strncpy(pNext, str, remaining);
		
		*staticStr = pNext;
		pNext += len;
		remaining -= len;
	OscFunctionEnd()
	
#if defined(TARGET_TYPE_LEANXCAM) || (TARGET_TYPE_INDXCAM)
	OscFunction(hasBayernPattern, struct OscSystemInfo * pInfo, bool * res)
		if (pInfo->hardware.board.boardType == OscSystemInfo_boardType_leanXcam) {
			if (strcmp(pInfo->hardware.board.assembly, "A") == 0 || strcmp(pInfo->hardware.board.assembly, "B") == 0) {
				*res = true;
			} else if (strcmp(pInfo->hardware.board.assembly, "C") == 0) {
				*res = false;
			} else {
				OscFail();
			}
		} else if (pInfo->hardware.board.boardType == OscSystemInfo_boardType_indXcam) {
			*res = false;
		} else {
			OscFail();
		}
	OscFunctionEnd()
#endif /* TARGET_TYPE_LEANXCAM || TARGET_TYPE_INDXCAM*/
	
	static struct OscSystemInfo info = { };
	static bool inited = false;
	
	if (!inited) {
		char * envVar;
		char envVar2[80];
		
		OscCall(getUBootEnv, "hwrev", &envVar)
		if (OscLastStatus() == ECFG_UBOOT_ENV_NOT_FOUND) {
			OscCall(getUBootEnv, "HWREV", &envVar); // Fallback to the ALL_CAPS_VARIANT.
			if (OscLastStatus() == ECFG_UBOOT_ENV_NOT_FOUND) {
#ifdef TARGET_TYPE_LEANXCAM
				envVar = "LX_1.1_B";
#endif
#ifdef TARGET_TYPE_INDXCAM
				envVar = "IX_1.1_A";
#endif
#ifdef TARGET_TYPE_LEANXRADIO
				envVar = "LEANXRADIO_1.0_A";
#endif
			}
		}
		
		// FIXME: why do I have to copy the string out of getUBootEnv's static buffer?
		strcpy(envVar2, envVar);
		envVar = envVar2;
		
		OscCall(staticStore, envVar, &info.hardware.board.revision);
		
		{
			char * part2, * part3, * part4;
			
			part2 = strchr(envVar, '_');
			OscAssert_m(part2 != NULL, "Invalid format for hwrev: %s", info.hardware.board.revision);
			*part2 = '\0';
			part2 += 1;
			
			part3 = strchr(part2, '.');
			OscAssert_m(part3 != NULL, "Invalid format for hwrev: %s", info.hardware.board.revision);
			*part3 = 0;
			part3 += 1;
			
			part4 = strchr(part3, '_');
			OscAssert_m(part4 != NULL, "Invalid format for hwrev: %s", info.hardware.board.revision);
			*part4 = 0;
			part4 += 1;
			
			if (strcmp(envVar, "LX") == 0)
				info.hardware.board.boardType = OscSystemInfo_boardType_leanXcam;
			else if (strcmp(envVar, "IX") == 0)
				info.hardware.board.boardType = OscSystemInfo_boardType_indXcam;
			else if (strcmp(envVar, "LEANXRADIO") == 0)
				info.hardware.board.boardType = OscSystemInfo_boardType_leanXradio;
			else
				OscAssert_m(part2 != NULL, "Invalid format for hwrev: %s", info.hardware.board.revision);
			
			OscCall(parseInteger, part2, &info.hardware.board.major);
			OscCall(parseInteger, part3, &info.hardware.board.minor);
			
			OscCall(staticStore, part4, &info.hardware.board.assembly);
		}

#if defined(TARGET_TYPE_LEANXCAM) || (TARGET_TYPE_INDXCAM)
		info.hardware.imageSensor.imageWidth = OSC_CAM_MAX_IMAGE_WIDTH;
		info.hardware.imageSensor.imageWidth = OSC_CAM_MAX_IMAGE_HEIGHT;
		OscCall(hasBayernPattern, &info, &info.hardware.imageSensor.hasBayernPattern);
#endif /* TARGET_TYPE_LEANXCAM || TARGET_TYPE_INDXCAM*/
		
		{
			char * version;
			
			info.software.Oscar.major = OSC_VERSION_MAJOR;
			info.software.Oscar.minor = OSC_VERSION_MINOR;
			info.software.Oscar.patch = OSC_VERSION_PATCH;
			info.software.Oscar.rc = OSC_VERSION_RC;
			
			OscCall(OscGetVersionString, &version);
			OscCall(staticStore, version, &info.software.Oscar.version);
			
			OscCall(getUClinuxVersion, &version);
			OscCall(staticStore, version, &info.software.uClinux.version);
		}
		
		inited = true;
	}
	
	*ppInfo = &info;
OscFunctionEnd()

/*======================= Private methods ==============================*/
// FIXME: Private? Why aren't these static then?
OSC_ERR OscCfgFlushContentHelper(const CFG_FILE_CONTENT_HANDLE hFileContent, bool all)
{
	FILE        *pCfgFile;
	char        *strFileName;
	size_t      fileSize;
	unsigned int strSize;
	unsigned int index;

	/* check preconditions */
	if(hFileContent == 0 || hFileContent > CONFIG_FILE_MAX_NUM)
	{
		OscLog(ERROR, "%s(%d): Invalid parameter.\n",
				__func__, hFileContent);
		return -ECFG_INVALID_FUNC_PARAMETER;
	}
	index = hFileContent-1;
	strSize = strlen(cfg.contents[index].data); /* string size without \0 */
	if (strSize > cfg.contents[index].dataSize - 1)
		{
		OscLog(ERROR, "%s: invalid content size!\n",
				__func__);
		return -ECFG_ERROR;
	}

	/* open file */
	strFileName = cfg.contents[index].fileName;
	pCfgFile = fopen(strFileName, "w+");
	if(pCfgFile == NULL)
	{
		OscLog(ERROR, "%s: Unable to open config file %s!\n",
				__func__, strFileName);
		return -ECFG_UNABLE_TO_OPEN_FILE;
	}
	
	fileSize = fwrite(cfg.contents[index].data, sizeof(char), strSize, pCfgFile);   /* write string */
	if (all)
	{
		memset(&cfg.contents[index].data[strSize], 0, cfg.contents[index].dataSize - strSize);
		fileSize += fwrite(&cfg.contents[index].data[strSize], sizeof(char), cfg.contents[index].dataSize - strSize - 1, pCfgFile);
	}
	fclose(pCfgFile);
	if (fileSize < strSize)
	{
		OscLog(ERROR, "%s: could not write data!\n",
				__func__);
		return -ECFG_UNABLE_TO_WRITE_FILE;
	}

/*    OscLog(DEBUG, "Wrote config file (%s):\n%s\n", strFileName, cfg.contents[index].data);*/

	return SUCCESS;
}

OSC_ERR OscCfgGetValPtr(
		const unsigned int  contentIndex,
		const struct CFG_KEY *pKey,
		char **pPStrVal)
{
	char            *pStrSecStart;  /* points to beginning of section content */
	
	/* check preconditions */
	if(pPStrVal == NULL ||
			pKey == NULL || pKey->strTag == NULL)
	{
		OscLog(ERROR, "%s(%d, 0x%x): Invalid parameter.\n",
				__func__, contentIndex, pKey->strTag);
		return -ECFG_INVALID_FUNC_PARAMETER;
	}
	
	/* find section */
	pStrSecStart = OscCfgFindNewlineLabel(pKey->strSection, CONFIG_FILE_SECTION_SUFFIX, cfg.contents[contentIndex].data);
	if(pStrSecStart == NULL)
	{
		*pPStrVal = NULL;
		return SUCCESS;
	}

	/* find tag */
	*pPStrVal = OscCfgFindNewlineLabel(pKey->strTag, CONFIG_FILE_TAG_SUFFIX, pStrSecStart);
	return SUCCESS;
}

char* OscCfgIsSubStr(
		const char *subString,
		const size_t subStringLen,
		const char *string)
{
	unsigned int i = 0;
	/* check preconditions */
	if (subString==NULL || string==NULL)
	{
		OscLog(ERROR, "%s(0x%x, %d, 0x%x): Invalid parameter.\n",
				__func__, subString, subStringLen, string);
		return NULL;
	}
	for (i=0; i<subStringLen; i++)
	{
		if ((subString[i] != string[i]) ||
			(subString[i] == 0) ||
			(string[i] == 0))
		{
			return NULL;
		}
	}
	return (char*)&string[i];
}

char* OscCfgFindNewlineLabel(
		const char* label,
		const char* labelSuffix,
		char* text)
{
	const char newLine = '\n';
	char* textStr, *tmpStr;
	unsigned int offset;
	size_t labelLen, labelSuffixLen;

	/* check preconditions */
	if (text == NULL || labelSuffix == NULL)
	{
		OscLog(ERROR, "%s(0x%x, 0x%x, 0x%x): Invalid parameter.\n",
				__func__, label, labelSuffix, text);
		return NULL;
	}
	/* no label is always found at the beginning of the text */
	if (label == NULL)
	{
		return text;
	}

	labelLen = strlen(label);
	labelSuffixLen = strlen(labelSuffix);
	offset = 0;
	for (textStr = text; textStr!=NULL; textStr = strchr(textStr, newLine))
	{
		/* find label */
		textStr = &textStr[offset];
		offset = 1;
		tmpStr = OscCfgIsSubStr(label, labelLen, textStr);
		if (tmpStr != NULL)
		{
			/* find label suffix */
			tmpStr = OscCfgIsSubStr(labelSuffix, labelSuffixLen, tmpStr);
			if (tmpStr != NULL)
			{
				return tmpStr;
			}
		}
	}

	return NULL;
}

OSC_ERR OscCfgReplaceStr(
		const unsigned int  contentIndex,
		const char *oldStr,
		const char *newStr,
		char* text)
{
	size_t newStrLen, oldStrLen, textLen, diffLen;
	int16 i;

	/* check preconditions */
	if (newStr == NULL || oldStr == NULL || text == NULL)
	{
		OscLog(ERROR, "%s(%d, 0x%x, 0x%x, 0x%x): Invalid parameter.\n",
				__func__, oldStr, newStr, text);
		return -ECFG_ERROR;
	}
	newStrLen = strlen(newStr);
	oldStrLen = strlen(oldStr);
	textLen = strlen(text);

	/* make space for newStr in text */
	if (newStrLen > oldStrLen)
	{
		/* shift text right, start from end  */
		diffLen = newStrLen - oldStrLen;
		/* check maximum file size */
		if (diffLen + strlen(cfg.contents[contentIndex].data) > cfg.contents[contentIndex].dataSize)
		{
			OscLog(ERROR, "%s: file length exceeded!\n",
					__func__);
			return -ECFG_ERROR;
		}
		for (i=(int16)textLen; i >= (int16)oldStrLen; i--)
		{
			text[i+diffLen] = text[i];
		}
	}
	else
	{
		/* shift text left, start from beginning */
		diffLen = oldStrLen - newStrLen;
		for (i=(unsigned int)newStrLen; i<textLen + 1; i++)
		{
			text[i] = text[i+diffLen];
		}
	}
	/* insert newStr (faster than strcpy because sizes are known here) */
	for (i=0; i<newStrLen; i++)
	{
		text[i] = newStr[i];
	}
	return SUCCESS;
}

char* OscCfgAppendLabel(
		char* text,
		const unsigned int maxTextLen,
		const char* label,
		const char* labelPrefix,
		const char* labelSuffix)
{
	/* check preconditions */
	if (text == NULL || labelPrefix == NULL || labelSuffix == NULL)
	{
		OscLog(ERROR, "%s(0x%x, %d, 0x%x, 0x%x, 0x%x): Invalid parameter.\n",
				__func__, text, maxTextLen, label, labelPrefix, labelSuffix);
		return NULL;
	}
	
	/* do nothing if label == NULL */
	if (label == NULL)
	{
		return text;
	}

	/* check file size */
	if(strlen(text) + strlen(label) + strlen(labelSuffix) + 1 > maxTextLen)
	{
		OscLog(ERROR, "%s: cannot insert label '%s'; file length exceeded!\n",
				__func__, label);
		return NULL;
	}
	/* add label string to end of file */
	strcat(text, labelPrefix);
	strcat(text, label);
	strcat(text, labelSuffix);
	return &text[strlen(text)];
}

unsigned int OscCfgFindInvalidChar(const unsigned char *str, const unsigned int strSize)
{
	int i;
	for (i=0; i<strSize; i++)
	{
		if ((str[i] < (unsigned char)0x0a) || (str[i] > (unsigned char)0x7f))
		{
			return i;
		}
	}
	return strSize;
}

OscFunction(static getUBootEnv, char * key, char ** value)
#ifdef OSC_HOST
	*value = NULL;
	OscFail_es(ECFG_UBOOT_ENV_NOT_FOUND);
#endif
#ifdef OSC_TARGET
	static char buffer[80];
	
	{
		FILE * file;
		char * ferr;
		int err;
		
		// FIXME: key should be escaped or checked for invalid characters.
		err = snprintf(buffer, sizeof buffer, "fw_printenv '%s' 2> /dev/null", key);
		OscAssert_em(err < sizeof buffer, -ECFG_UBOOT_ENV_READ_ERROR, "No buffer space left.");
		
		file = popen(buffer, "r");
		OscAssert_em(file != NULL, -ECFG_UBOOT_ENV_READ_ERROR, "Error starting command: %s", strerror(errno));
		
		ferr = fgets(buffer, sizeof buffer, file);
		
		if (feof(file) != 0)
			OscFail_es(ECFG_UBOOT_ENV_NOT_FOUND);
			
		err = pclose(file);
		
		OscAssert_em(ferr != NULL || feof(file) != 0, -ECFG_UBOOT_ENV_READ_ERROR, "Error reading from command: %s", strerror(errno));
		OscAssert_em(err != -1, -ECFG_UBOOT_ENV_READ_ERROR, "Error closing command: %s", strerror(errno));
		
		if (err == 1) {
			*value = NULL;
			OscFail_es(ECFG_UBOOT_ENV_NOT_FOUND);
		} else if (err != 0) {
			OscFail_em(-ECFG_UBOOT_ENV_READ_ERROR, "Error in command: %d", err);
		}
	}
	
	{
		char * equals, * newline;
		
		newline = strchr(buffer, '\n');
		OscAssert_em(newline != NULL, -ECFG_UBOOT_ENV_READ_ERROR, "No newline found.");
		*newline = 0;
		
		equals = strchr(buffer, '=');
		OscAssert_em(equals != NULL, -ECFG_UBOOT_ENV_READ_ERROR, "No equals sign found.");
		*value = equals + 1;
	}
#endif
OscFunctionEnd()

OscFunction(static parseInteger, char * str, int * res)
	char * pEnd;
	int num = strtol(str, &pEnd, 10);
	
	OscAssert_m(str != pEnd, "Not a valid integer: %s", str);
	OscAssert_m(*pEnd == 0, "Garbage at end of integer: %s", str);
	
	*res = num;
OscFunctionEnd()

OscFunction(static getUClinuxVersion, char ** res)
	FILE * file = NULL;
	
	int err;
	int ret;
	int major=0, minor=0, patch_level=0, rc=0;
	char* next = NULL;
	char* occur = NULL;
	
	file=fopen("/proc/version", "r");
				
	OscAssert(file != NULL);
	
	static char version[200];
	fread(version, sizeof(char), 200, file);
	
	occur=strstr(version, "Git_");
	OscAssert(occur!=NULL);
	occur+=4;

	ret = sscanf(occur, "v%d.%d-p%d-RC%d%*s", &major, &minor, &patch_level, &rc);
	if(ret == 4 && major >= 0 && minor >= 0 && patch_level >= 0 && rc >= 0)
	{
		// Version number of form v1.2-p1-RC2
		next = strstr(occur, "RC");
		next += 2 + (1 + patch_level/10);
		goto cleanup_and_exit;
	}

	ret = sscanf(occur, "v%d.%d-RC%d%*s", &major, &minor, &rc);
	if(ret == 3 && major >= 0 && minor >= 0 && patch_level >= 0 && rc >= 0)
	{
		// Version number of form v1.2-RC2
		next = strstr(occur, "RC");
		next += 2 + (1 + patch_level/10);
		goto cleanup_and_exit;
	}

	ret = sscanf(occur, "v%d.%d-p%d%*s", &major, &minor, &patch_level);
	if(ret == 3 && major >= 0 && minor >= 0 && patch_level >= 0)
	{
		// Version number of form v1.2-p1
		next = strstr(occur, "-p");
		next += 2 + (1 + patch_level/10);
		goto cleanup_and_exit;
	}

	ret = sscanf(occur, "v%d.%d%*s", &major, &minor);
	if(ret == 2 && major >= 0 && minor >= 0)
	{
		// Version umber of form v1.2
		next = strstr(occur, ".");
		next += 1 + (1 + minor/10);
		goto cleanup_and_exit;
	}
	
	// Not able to parse => No valid version found.
	OscLog(ERROR, "No valid uCLinux version string found!\n");
	*res = "v0.0-p0";

cleanup_and_exit:
	OscAssert(next != NULL);
	*next=0;
	OscAssert(occur != NULL);
	*res=occur;

	err = fclose(file);
	OscAssert(err == 0);
	
OscFunctionCatch()
//	fclose(file); FIXME: File's not in scope anymore!
	*res = "v0.0-p0";
OscFunctionEnd()

// FIXME: This file is too long! (Written on line 1018)
