///////////////////////////////////////////////////////////////////////////////
#include "sakeRequestInternal.h"
#include "sakeRequest.h"



static SAKEStartRequestResult SAKE_CALL sakeiRateRecordValidateInput(SAKERequest request)
{
	SAKERateRecordInput *input = (SAKERateRecordInput *)request->mInput;

	if(!input->mTableId)
		return SAKEStartRequestResult_BAD_TABLEID;

	return SAKEStartRequestResult_SUCCESS;
}

static SAKEStartRequestResult SAKE_CALL sakeiRateRecordFillSoapRequest(SAKERequest request)
{
	SAKERateRecordInput *input = (SAKERateRecordInput *)request->mInput;

	gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "tableid", input->mTableId);

	gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "recordid", (gsi_u32)input->mRecordId);

	gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "rating", (gsi_u32)input->mRating);

	return SAKEStartRequestResult_SUCCESS;
}

static SAKERequestResult sakeiRateRecordProcessSoapResponse(SAKERequest request)
{
	SAKERateRecordOutput *output = (SAKERateRecordOutput *)request->mOutput;

	if(gsi_is_false(gsXmlReadChildAsInt(request->mSoapResponse, "numRatings", &output->mNumRatings)) ||
	   gsi_is_false(gsXmlReadChildAsFloat(request->mSoapResponse, "averageRating", &output->mAverageRating)))
	{
		return SAKERequestResult_MALFORMED_RESPONSE;
	}

	return SAKERequestResult_SUCCESS;
}

SAKEStartRequestResult SAKE_CALL sakeiStartRateRecordRequest(SAKERequest request)
{
	static SAKEIRequestInfo info =
	{
		sizeof(SAKERateRecordOutput),
		SAKEI_FUNC_NAME_STRINGS("RateRecord"),
		sakeiRateRecordValidateInput,
		sakeiRateRecordFillSoapRequest,
		sakeiRateRecordProcessSoapResponse
	};

	return sakeiStartRequest(request, &info);
}



static SAKEStartRequestResult SAKE_CALL sakeiGetRecordLimitValidateInput(SAKERequest request)
{
	SAKEGetRecordLimitInput *input = (SAKEGetRecordLimitInput *)request->mInput;

	if(!input->mTableId)
		return SAKEStartRequestResult_BAD_TABLEID;

	return SAKEStartRequestResult_SUCCESS;
}

static SAKEStartRequestResult SAKE_CALL sakeiGetRecordLimitFillSoapRequest(SAKERequest request)
{
	SAKEGetRecordLimitInput *input = (SAKEGetRecordLimitInput *)request->mInput;

	gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "tableid", input->mTableId);

	return SAKEStartRequestResult_SUCCESS;
}

static SAKERequestResult sakeiGetRecordLimitProcessSoapResponse(SAKERequest request)
{
	SAKEGetRecordLimitOutput *output = (SAKEGetRecordLimitOutput *)request->mOutput;

	if(gsi_is_false(gsXmlReadChildAsInt(request->mSoapResponse, "limitPerOwner", &output->mLimitPerOwner)) ||
	   gsi_is_false(gsXmlReadChildAsInt(request->mSoapResponse, "numOwned", &output->mNumOwned)))
	{
		return SAKERequestResult_MALFORMED_RESPONSE;
	}

	return SAKERequestResult_SUCCESS;
}

SAKEStartRequestResult SAKE_CALL sakeiStartGetRecordLimitRequest(SAKERequest request)
{
	static SAKEIRequestInfo info =
	{
		sizeof(SAKEGetRecordLimitOutput),
		SAKEI_FUNC_NAME_STRINGS("GetRecordLimit"),
		sakeiGetRecordLimitValidateInput,
		sakeiGetRecordLimitFillSoapRequest,
		sakeiGetRecordLimitProcessSoapResponse
	};

	return sakeiStartRequest(request, &info);
}



static SAKEStartRequestResult SAKE_CALL sakeiGetRecordCountValidateInput(SAKERequest request)
{
	SAKEGetRecordCountInput *input = (SAKEGetRecordCountInput *)request->mInput;

	if(!input->mTableId)
		return SAKEStartRequestResult_BAD_TABLEID;

	return SAKEStartRequestResult_SUCCESS;
}

static SAKEStartRequestResult SAKE_CALL sakeiGetRecordCountFillSoapRequest(SAKERequest request)
{
	SAKEGetRecordCountInput *input = (SAKEGetRecordCountInput *)request->mInput;

	gsXmlWriteStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "tableid", input->mTableId);

	if(input->mFilter != NULL)
		gsXmlWriteTStringElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "filter", input->mFilter);

	gsXmlWriteIntElement(request->mSoapRequest, GSI_SAKE_SERVICE_NAMESPACE, "cacheFlag", (gsi_u32)input->mCacheFlag);


	return SAKEStartRequestResult_SUCCESS;
}

static SAKERequestResult sakeiGetRecordCountProcessSoapResponse(SAKERequest request)
{
	SAKEGetRecordCountOutput *output = (SAKEGetRecordCountOutput *)request->mOutput;

	if(gsi_is_false(gsXmlReadChildAsInt(request->mSoapResponse, "count", &output->mCount)))
	{
		return SAKERequestResult_MALFORMED_RESPONSE;
	}

	return SAKERequestResult_SUCCESS;
}


SAKEStartRequestResult SAKE_CALL sakeiStartGetRecordCountRequest(SAKERequest request)
{
	static SAKEIRequestInfo info =
	{
		sizeof(SAKEGetRandomRecordOutput),
		SAKEI_FUNC_NAME_STRINGS("GetRecordCount"),
		sakeiGetRecordCountValidateInput,
		sakeiGetRecordCountFillSoapRequest,
		sakeiGetRecordCountProcessSoapResponse
	};

	return sakeiStartRequest(request, &info);
}
