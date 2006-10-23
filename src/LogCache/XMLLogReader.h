#pragma once

///////////////////////////////////////////////////////////////
// necessary includes
///////////////////////////////////////////////////////////////

#include "CachedLogInfo.h"

///////////////////////////////////////////////////////////////
//
// CXMLLogReader
//
//		utility class to parse an XML formatted log and add
//		the data to the given changed log info.
//
///////////////////////////////////////////////////////////////

class CXMLLogReader
{
private:

	// a strstr-like utility that works on memory buffers
	// (i.e. without zero termination)

	static const char* limited_strstr ( const char* first
								      , const char* last
								      , const char* sub
							          , size_t subLen);

	// return the first position after the opening tag name
	// and the position of the opening angle bracket of the
	// corresponding end tag. Returns false, if no tag was
	// found in [start .. parentEnd)

	static bool GetXMLTag ( const char* start
						  , const char* parentEnd
						  , const char* startTagName
						  , size_t startTagNameLen
						  , const char* endTagName
						  , size_t endTagNameLen
						  , const char*& tagStart
						  , const char*& tagEnd);

	// specialization for the <log> tag as we want to skip
	// most of the file, if possible (we know the closing
	// tag will be near the end of the file)

	static bool GetLogTag ( const char* start
						  , const char* parentEnd
						  , const char*& tagStart
						  , const char*& tagEnd);

	// get attribute values from a range returned by GetXMLTag

	static const char* GetXMLAttributeOffset ( const char* start
											 , const char* end
											 , const char* attribute
											 , size_t attributeLen);
	static int GetXMLRevisionAttribute ( const char* start
									   , const char* end
									   , const char* attribute
									   , size_t attributeLen);
	static std::string GetXMLTextAttribute ( const char* start
										   , const char* end
										   , const char* attribute
										   , size_t attributeLen);

	// get the text between the given tags (e.g. for <path>)

	static std::string GetXMLTaggedText ( const char* start
										, const char* end
										, const char* startTagName
										, size_t startTagNameLen
										, const char* endTagName
										, size_t endTagNameLen);

	// parse all <path> tags within a <logentry> tag

	static void ParseChanges ( const char* current
						     , const char* changesEnd
							 , CCachedLogInfo& target);

	// parse all <logentry> tags

	static void ParseXMLLog ( const char* current
						    , const char* logEnd
							, CCachedLogInfo& target);

public:

	// for convenience

	typedef CRevisionInfoContainer::TChangeAction TChangeAction;

	// map file to memory, parse it and fill the target

	static void LoadFromXML ( const std::wstring& xmlFileName
						    , CCachedLogInfo& target);
};

