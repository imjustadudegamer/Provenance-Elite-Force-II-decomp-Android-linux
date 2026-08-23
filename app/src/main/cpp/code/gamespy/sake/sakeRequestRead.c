#include "sakeRequestInternal.h"
#include "sakeRequest.h"


static SAKEStartRequestResult SAKE_CALL sakeiValidateRequestFieldNames(char **fields, int numFields)
{
	int i;

	for(i = 0 ; i < numFields ; i++)
	{
		if(!fields[i] || !fields[i][0])
			return SAKEStartRequestResult_BAD_FIELD_NAME;
	}

	return SAKEStartRequestResult_SUCCESS;
}


static void SAKE_CALL sakeiFillSoapRequestFieldNames(SAKERequest request, char **names, int numFields)
{
	int i;

	gsXmlWriteOpenTag(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "fields");

	for(i = 0 ; i < numFields ; i++)
	{
		gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "string", names[i]);
	}

	gsXmlWriteCloseTag(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "fields");
}


static void SAKE_CALL sakeiFillSoapRequestRecordIds(SAKERequest request, int *recordIds, int numRecordIds)
{
	int i;

	gsXmlWriteOpenTag(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "recordids");

	for(i = 0 ; i < numRecordIds ; i++)
	{
		gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "int", (gsi_u32)recordIds[i]);
	}

	gsXmlWriteCloseTag(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "recordids");
}


static void SAKE_CALL sakeiFillSoapRequestOwnerIds(SAKERequest request, int *ownerIds, int numOwnerIds)
{
	int i;

	gsXmlWriteOpenTag(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "ownerids");

	for(i = 0 ; i < numOwnerIds ; i++)
	{
		gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "int", (gsi_u32)ownerIds[i]);
	}

	gsXmlWriteCloseTag(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "ownerids");
}


static SAKERequestResult SAKE_CALL sakeiReadOutputRecords(SAKERequest request, SAKEField ***outputRecordsPtr, int *numRecords,
                                                          int numFields, char ** fieldNames)

{
	SAKEField **outputRecords;
	SAKEField *outputRecord;
	SAKEField *outputField;
	int recordIndex;
	int fieldIndex;
	size_t size;

	if(gsi_is_false(gsXmlMoveToChild(request->mSoapResponse, "values")))
		return SAKERequestResult_MALFORMED_RESPONSE;

	*numRecords = gsXmlCountChildren(request->mSoapResponse, "ArrayOfRecordValue");

	if(*numRecords == 0)
	{
		*outputRecordsPtr = NULL;
		return SAKERequestResult_SUCCESS;
	}

	size = (sizeof(SAKEField*) * *numRecords);
	outputRecords = (SAKEField**)gsimalloc(size);
	if(!outputRecords)
		return SAKERequestResult_OUT_OF_MEMORY;
	memset(outputRecords, 0, size);
	*outputRecordsPtr = outputRecords;

	for(recordIndex = 0 ; recordIndex < *numRecords ; recordIndex++)
	{
		if(gsi_is_false(gsXmlMoveToNext(request->mSoapResponse, "ArrayOfRecordValue")))
			return SAKERequestResult_MALFORMED_RESPONSE;

		size = (sizeof(SAKEField) * numFields);
		outputRecord = (SAKEField*)gsimalloc(size);
		if(!outputRecord)
			return SAKERequestResult_OUT_OF_MEMORY;
		memset(outputRecord, 0, size);
		outputRecords[recordIndex] = outputRecord;

		if(gsXmlCountChildren(request->mSoapResponse, "RecordValue") != numFields)
			return SAKERequestResult_MALFORMED_RESPONSE;

		for(fieldIndex = 0 ; fieldIndex < numFields ; fieldIndex++)
		{
			outputField = &outputRecord[fieldIndex];

			outputField->mName = fieldNames[fieldIndex];

			if(gsi_is_false(gsXmlMoveToNext(request->mSoapResponse, "RecordValue")))
				return SAKERequestResult_MALFORMED_RESPONSE;

			if(gsXmlMoveToChild(request->mSoapResponse, "byteValue"))
			{
				int value;
				if(gsi_is_false(gsXmlReadChildAsInt(request->mSoapResponse, "value", &value)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				outputField->mType = SAKEFieldType_BYTE;
				outputField->mValue.mByte = (gsi_u8)value;
			}
			else if(gsXmlMoveToChild(request->mSoapResponse, "shortValue"))
			{
				int value;
				if(gsi_is_false(gsXmlReadChildAsInt(request->mSoapResponse, "value", &value)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				outputField->mType = SAKEFieldType_SHORT;
				outputField->mValue.mShort = (gsi_i16)value;
			}
			else if(gsXmlMoveToChild(request->mSoapResponse, "intValue"))
			{
				int value;
				if(gsi_is_false(gsXmlReadChildAsInt(request->mSoapResponse, "value", &value)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				outputField->mType = SAKEFieldType_INT;
				outputField->mValue.mInt = (gsi_i32)value;
			}
			else if (gsXmlMoveToChild(request->mSoapResponse, "int64Value"))
			{
				gsi_i64 value;
				if (gsi_is_false(gsXmlReadChildAsInt64(request->mSoapResponse, "value", &value)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				outputField->mType = SAKEFieldType_INT64;
				outputField->mValue.mInt64 = value;
			}
			else if(gsXmlMoveToChild(request->mSoapResponse, "floatValue"))
			{
				float value;
				if(gsi_is_false(gsXmlReadChildAsFloat(request->mSoapResponse, "value", &value)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				outputField->mType = SAKEFieldType_FLOAT;
				outputField->mValue.mFloat = value;
			}
			else if(gsXmlMoveToChild(request->mSoapResponse, "asciiStringValue"))
			{
				char *value;
				int len;
				if(gsi_is_false(gsXmlReadChildAsString(request->mSoapResponse, "value", (const char**)&value, &len)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				if(value)
					value[len] = '\0';
				else
					value = "";
				outputField->mType = SAKEFieldType_ASCII_STRING;
				outputField->mValue.mAsciiString = value;
			}
			else if(gsXmlMoveToChild(request->mSoapResponse, "unicodeStringValue"))
			{
				char *value;
				gsi_u16 *valueUnicode;
				int len;
				if(gsi_is_false(gsXmlReadChildAsString(request->mSoapResponse, "value", (const char**)&value, &len)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				if(value)
					value[len] = '\0';
				else
					value = "";
				valueUnicode = UTF8ToUCS2StringAlloc(value);
				if(!valueUnicode)
					return SAKERequestResult_OUT_OF_MEMORY;
				outputField->mType = SAKEFieldType_UNICODE_STRING;
				outputField->mValue.mUnicodeString = valueUnicode;
			}
			else if(gsXmlMoveToChild(request->mSoapResponse, "booleanValue"))
			{
				char *value;
				int len;
				gsi_bool boolval;
				if(gsi_is_false(gsXmlReadChildAsString(request->mSoapResponse, "value", (const char**)&value, &len)))
					return SAKERequestResult_MALFORMED_RESPONSE;	
				if(value)
				{
					value[len] = '\0';
					boolval = (strcmp(value,"true") == 0)?gsi_true:gsi_false;
				}
				else
					boolval = gsi_false; 
				outputField->mType = SAKEFieldType_BOOLEAN;
				outputField->mValue.mBoolean = boolval;
			}
			else if(gsXmlMoveToChild(request->mSoapResponse, "dateAndTimeValue"))
			{
				time_t value;
				if(gsi_is_false(gsXmlReadChildAsDateTimeElement(request->mSoapResponse, "value", &value)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				outputField->mType = SAKEFieldType_DATE_AND_TIME;
				outputField->mValue.mDateAndTime = value;
			}
			else if(gsXmlMoveToChild(request->mSoapResponse, "binaryDataValue"))
			{
				gsi_u8 *value;
				int len;
				if(gsi_is_false(gsXmlReadChildAsBase64Binary(request->mSoapResponse, "value", NULL, &len)))
					return SAKERequestResult_MALFORMED_RESPONSE;
				if(len > 0)
				{
					value = (gsi_u8*)gsimalloc((size_t)len);
					if(!value)
						return SAKERequestResult_OUT_OF_MEMORY;
					if(gsi_is_false(gsXmlReadChildAsBase64Binary(request->mSoapResponse, "value", value, &len)))
					{
						gsifree(value);
						return SAKERequestResult_MALFORMED_RESPONSE;
					}
				}
				else
				{
					value = NULL;
					len = 0;
				}
				outputField->mType = SAKEFieldType_BINARY_DATA;
				outputField->mValue.mBinaryData.mLength = len;
				outputField->mValue.mBinaryData.mValue = value;
			}
			else
			{
				GS_FAIL_STR("No recognized field type found in RecordValue");
				return SAKERequestResult_UNKNOWN_ERROR;
			}
		}
	}

	return SAKERequestResult_SUCCESS;
}


static void SAKE_CALL sakeiFreeOutputRecord(int numFields, SAKEField *record)
{
	int i;

	if(!record)
		return;


	for (i = 0; i < numFields; i++)
	{
		if (record[i].mType == SAKEFieldType_BINARY_DATA && record[i].mValue.mBinaryData.mValue != NULL)
			gsifree(record[i].mValue.mBinaryData.mValue);
		if (record[i].mType == SAKEFieldType_UNICODE_STRING)
			gsifree(record[i].mValue.mUnicodeString);
	}
	gsifree(record);
}


static void SAKE_CALL sakeiFreeOutputRecords(int numFields, int numRecords, SAKEField **records)
{
	int i,j;

	if(!records)
		return;

	for(i = 0 ; i < numRecords ; i++)
	{
		for (j = 0; j < numFields; j++)
		{
			if (records[i][j].mType == SAKEFieldType_BINARY_DATA && records[i][j].mValue.mBinaryData.mValue != NULL)
				gsifree(records[i][j].mValue.mBinaryData.mValue);
			if (records[i][j].mType == SAKEFieldType_UNICODE_STRING)
				gsifree(records[i][j].mValue.mUnicodeString);
		}

		gsifree(records[i]);
	}
	gsifree(records);
}



static SAKEStartRequestResult SAKE_CALL sakeiSearchForRecordsValidateInput(SAKERequest request)
{
	SAKESearchForRecordsInput *input = (SAKESearchForRecordsInput *)request->mInput;

	if(!input->mTableId)
		return SAKEStartRequestResult_BAD_TABLEID;

	if(input->mNumFields <= 0)
		return SAKEStartRequestResult_BAD_NUM_FIELDS;

	if(!input->mFieldNames)
		return SAKEStartRequestResult_BAD_FIELDS;

	if(input->mOffset < 0)
		return SAKEStartRequestResult_BAD_OFFSET;

	if(input->mMaxRecords <= 0)
		return SAKEStartRequestResult_BAD_MAX; 

	return sakeiValidateRequestFieldNames(input->mFieldNames, input->mNumFields);
}

static SAKEStartRequestResult SAKE_CALL sakeiSearchForRecordsFillSoapRequest(SAKERequest request)
{
	SAKESearchForRecordsInput *input = (SAKESearchForRecordsInput *)request->mInput;

	gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "tableid", input->mTableId);

	if(input->mFilter != NULL)
		gsXmlWriteTStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "filter", input->mFilter);

	if(input->mSort != NULL)
		gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "sort", input->mSort);

	gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "offset", (gsi_u32)input->mOffset);

	gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "max", (gsi_u32)input->mMaxRecords);

	if(input->mTargetRecordFilter != NULL)
		gsXmlWriteTStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "targetfilter", input->mTargetRecordFilter);

	gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "surrounding", (gsi_u32)input->mSurroundingRecordsCount);

	sakeiFillSoapRequestOwnerIds(request, input->mOwnerIds, input->mNumOwnerIds);

	gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "cacheFlag", (gsi_u32)input->mCacheFlag);


	sakeiFillSoapRequestFieldNames(request, input->mFieldNames, input->mNumFields);

	return SAKEStartRequestResult_SUCCESS;
}

static SAKERequestResult sakeiSearchForRecordsProcessSoapResponse(SAKERequest request)
{
	SAKESearchForRecordsInput *input = (SAKESearchForRecordsInput *)request->mInput;
	SAKESearchForRecordsOutput *output = (SAKESearchForRecordsOutput *)request->mOutput;

	return sakeiReadOutputRecords(request, &output->mRecords, &output->mNumRecords, input->mNumFields, input->mFieldNames);
}

static void sakeiSearchForRecordsFreeData(SAKERequest request)
{
	SAKESearchForRecordsInput *input = (SAKESearchForRecordsInput *)request->mInput;
	SAKESearchForRecordsOutput *output = (SAKESearchForRecordsOutput *)request->mOutput;

	if(output)
		sakeiFreeOutputRecords(input->mNumFields, output->mNumRecords, output->mRecords);
}

SAKEStartRequestResult SAKE_CALL sakeiStartSearchForRecordsRequest(SAKERequest request)
{
	static SAKEIRequestInfo info =
	{
		sizeof(SAKESearchForRecordsOutput),
		SAKEI_FUNC_NAME_STRINGS("SearchForRecords"),
		sakeiSearchForRecordsValidateInput,
		sakeiSearchForRecordsFillSoapRequest,
		sakeiSearchForRecordsProcessSoapResponse,
		sakeiSearchForRecordsFreeData
	};

	return sakeiStartRequest(request, &info);
}



static SAKEStartRequestResult SAKE_CALL sakeiGetMyRecordsValidateInput(SAKERequest request)
{
	SAKEGetMyRecordsInput *input = (SAKEGetMyRecordsInput *)request->mInput;

	if(!input->mTableId)
		return SAKEStartRequestResult_BAD_TABLEID;

	if(input->mNumFields <= 0)
		return SAKEStartRequestResult_BAD_NUM_FIELDS;

	if(!input->mFieldNames)
		return SAKEStartRequestResult_BAD_FIELDS;

	return sakeiValidateRequestFieldNames(input->mFieldNames, input->mNumFields);
}

static SAKEStartRequestResult SAKE_CALL sakeiGetMyRecordsFillSoapRequest(SAKERequest request)
{
	SAKEGetMyRecordsInput *input = (SAKEGetMyRecordsInput *)request->mInput;

	gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "tableid", input->mTableId);

	sakeiFillSoapRequestFieldNames(request, input->mFieldNames, input->mNumFields);

	return SAKEStartRequestResult_SUCCESS;
}

static SAKERequestResult sakeiGetMyRecordsProcessSoapResponse(SAKERequest request)
{
	SAKEGetMyRecordsInput *input = (SAKEGetMyRecordsInput *)request->mInput;
	SAKEGetMyRecordsOutput *output = (SAKEGetMyRecordsOutput *)request->mOutput;

	return sakeiReadOutputRecords(request, &output->mRecords, &output->mNumRecords, input->mNumFields, input->mFieldNames);
}

static void sakeiGetMyRecordsFreeData(SAKERequest request)
{
	SAKEGetMyRecordsInput *input = (SAKEGetMyRecordsInput *)request->mInput;	
	SAKEGetMyRecordsOutput *output = (SAKEGetMyRecordsOutput *)request->mOutput;

	if(output)
		sakeiFreeOutputRecords(input->mNumFields, output->mNumRecords, output->mRecords);
}

SAKEStartRequestResult SAKE_CALL sakeiStartGetMyRecordsRequest(SAKERequest request)
{
	static SAKEIRequestInfo info =
	{
		sizeof(SAKEGetMyRecordsOutput),
		SAKEI_FUNC_NAME_STRINGS("GetMyRecords"),
		sakeiGetMyRecordsValidateInput,
		sakeiGetMyRecordsFillSoapRequest,
		sakeiGetMyRecordsProcessSoapResponse,
		sakeiGetMyRecordsFreeData
	};

	return sakeiStartRequest(request, &info);
}



static SAKEStartRequestResult SAKE_CALL sakeiGetSpecificRecordsValidateInput(SAKERequest request)
{
	SAKEGetSpecificRecordsInput *input = (SAKEGetSpecificRecordsInput *)request->mInput;

	if(!input->mTableId)
		return SAKEStartRequestResult_BAD_TABLEID;

	if(input->mNumRecordIds <= 0)
		return SAKEStartRequestResult_BAD_NUM_RECORDIDS;

	if(!input->mRecordIds)
		return SAKEStartRequestResult_BAD_RECORDIDS;

	if(input->mNumFields <= 0)
		return SAKEStartRequestResult_BAD_NUM_FIELDS;

	if(!input->mFieldNames)
		return SAKEStartRequestResult_BAD_FIELDS;

	return sakeiValidateRequestFieldNames(input->mFieldNames, input->mNumFields);
}

static SAKEStartRequestResult SAKE_CALL sakeiGetSpecificRecordsFillSoapRequest(SAKERequest request)
{
	SAKEGetSpecificRecordsInput *input = (SAKEGetSpecificRecordsInput *)request->mInput;

	gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "tableid", input->mTableId);

	sakeiFillSoapRequestRecordIds(request, input->mRecordIds, input->mNumRecordIds);

	sakeiFillSoapRequestFieldNames(request, input->mFieldNames, input->mNumFields);

	return SAKEStartRequestResult_SUCCESS;
}

static SAKERequestResult sakeiGetSpecificRecordsProcessSoapResponse(SAKERequest request)
{
	SAKEGetSpecificRecordsInput *input = (SAKEGetSpecificRecordsInput *)request->mInput;
	SAKEGetSpecificRecordsOutput *output = (SAKEGetSpecificRecordsOutput *)request->mOutput;

	return sakeiReadOutputRecords(request, &output->mRecords, &output->mNumRecords, input->mNumFields, input->mFieldNames);
}

static void sakeiGetSpecificRecordsFreeData(SAKERequest request)
{
	SAKEGetSpecificRecordsInput *input = (SAKEGetSpecificRecordsInput *)request->mInput;	
	SAKEGetSpecificRecordsOutput *output = (SAKEGetSpecificRecordsOutput *)request->mOutput;

	if(output)
		sakeiFreeOutputRecords(input->mNumFields, output->mNumRecords, output->mRecords);
}

SAKEStartRequestResult SAKE_CALL sakeiStartGetSpecificRecordsRequest(SAKERequest request)
{
	static SAKEIRequestInfo info =
	{
		sizeof(SAKEGetSpecificRecordsOutput),
		SAKEI_FUNC_NAME_STRINGS("GetSpecificRecords"),
		sakeiGetSpecificRecordsValidateInput,
		sakeiGetSpecificRecordsFillSoapRequest,
		sakeiGetSpecificRecordsProcessSoapResponse,
		sakeiGetSpecificRecordsFreeData
	};

	return sakeiStartRequest(request, &info);
}



static SAKEStartRequestResult SAKE_CALL sakeiGetRandomRecordValidateInput(SAKERequest request)
{
	SAKEGetRandomRecordInput *input = (SAKEGetRandomRecordInput *)request->mInput;

	if(!input->mTableId)
		return SAKEStartRequestResult_BAD_TABLEID;

	if(input->mNumFields <= 0)
		return SAKEStartRequestResult_BAD_NUM_FIELDS;

	if(!input->mFieldNames)
		return SAKEStartRequestResult_BAD_FIELDS;

	return sakeiValidateRequestFieldNames(input->mFieldNames, input->mNumFields);
}

static SAKEStartRequestResult SAKE_CALL sakeiGetRandomRecordFillSoapRequest(SAKERequest request)
{
	SAKEGetRandomRecordInput *input = (SAKEGetRandomRecordInput *)request->mInput;

	gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "tableid", input->mTableId);

	if(input->mFilter != NULL)
		gsXmlWriteTStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "filter", input->mFilter);

	gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "max", 1);

	sakeiFillSoapRequestFieldNames(request, input->mFieldNames, input->mNumFields);

	return SAKEStartRequestResult_SUCCESS;
}

static SAKERequestResult sakeiGetRandomRecordProcessSoapResponse(SAKERequest request)
{
	SAKEGetRandomRecordInput *input = (SAKEGetRandomRecordInput *)request->mInput;
	SAKEGetRandomRecordOutput *output = (SAKEGetRandomRecordOutput *)request->mOutput;
	SAKEField **records;
	int         numRecords;

	SAKERequestResult result = sakeiReadOutputRecords(request, &records, &numRecords, input->mNumFields, input->mFieldNames);
	if(result == SAKERequestResult_SUCCESS)
	{
		if((records != NULL) && (numRecords > 0))
			output->mRecord = records[0];
		else
			output->mRecord = NULL;
	}
	gsifree(records);

	return result;
}

static void sakeiGetRandomRecordFreeData(SAKERequest request)
{
	SAKEGetRandomRecordInput *input = (SAKEGetRandomRecordInput *)request->mInput;	
	SAKEGetRandomRecordOutput *output = (SAKEGetRandomRecordOutput *)request->mOutput;

	if(output)
		sakeiFreeOutputRecord(input->mNumFields, output->mRecord);

}

SAKEStartRequestResult SAKE_CALL sakeiStartGetRandomRecordRequest(SAKERequest request)
{
	static SAKEIRequestInfo info =
	{
		sizeof(SAKEGetRandomRecordOutput),
		SAKEI_FUNC_NAME_STRINGS("GetRandomRecords"),
		sakeiGetRandomRecordValidateInput,
		sakeiGetRandomRecordFillSoapRequest,
		sakeiGetRandomRecordProcessSoapResponse,
		sakeiGetRandomRecordFreeData
	};

	return sakeiStartRequest(request, &info);
}


