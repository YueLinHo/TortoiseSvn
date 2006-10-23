#include "StdAfx.h"
#include ".\XMLLogWriter.h"
#include ".\BufferedOutFile.h"

///////////////////////////////////////////////////////////////
// write <date> tag
///////////////////////////////////////////////////////////////

void CXMLLogWriter::WriteTimeStamp ( CBufferedOutFile& file
								   , __time64_t timeStamp)
{
	// empty time stamps don't show up in the log

	if (timeStamp == 0)
		return;

	// decode 64 bit time stamp

	int musecs = (int)(timeStamp % 1000000);
	timeStamp /= 1000000;
	const tm* time = _gmtime64 (&timeStamp);

	if (time == NULL)
		return;

	// fill the template & write to stream

	enum {BUFFER_SIZE = 100};
	char buffer[BUFFER_SIZE];

	_snprintf ( buffer
			  , BUFFER_SIZE
			  , "<date>%04d-%02d-%02dT%02d:%02d:%02d.%06dZ</date>\n"
			  , time->tm_year + 1900
			  , time->tm_mon + 1
			  , time->tm_mday
			  , time->tm_hour
			  , time->tm_min
			  , time->tm_sec
			  , musecs);

	file << buffer;
}

///////////////////////////////////////////////////////////////
// write <paths> tag
///////////////////////////////////////////////////////////////

void CXMLLogWriter::WriteChanges ( CBufferedOutFile& file
								 , CChangesIterator& iter	
								 , CChangesIterator& last)	
{
	// empty change sets won't show up in the log

	if (iter == last)
		return;

	// static text blocks

	static const std::string startText = "<paths>\n";
	static const std::string pathStartText = "<path";

	static const std::string copyFromPathText = "\n   copyfrom-path=\"";
	static const std::string copyFromRevisionText = "\"\n   copyfrom-rev=\"";
	static const std::string copyFromEndText = "\"";

	static const std::string addedActionText = "\n   action=\"A\"";
	static const std::string modifiedActionText = "\n   action=\"M\"";
	static const std::string replacedActionText = "\n   action=\"R\"";
	static const std::string deletedActionText = "\n   action=\"D\"";

	static const std::string pathText = ">";
	static const std::string pathEndText = "</path>\n";
	static const std::string endText = "</paths>\n";

	// write all changes

	file << startText;

	for (; iter != last; ++iter) 
	{
		file << pathStartText;

		if (iter->HasFromPath())
		{
			file << copyFromPathText;
			file << iter->GetFromPath().GetPath();
			file << copyFromRevisionText;
			file << iter->GetFromRevision();
			file << copyFromEndText;
		}

		switch (iter->GetAction())
		{
			case CRevisionInfoContainer::ACTION_CHANGED:
				file << modifiedActionText;
				break;
			case CRevisionInfoContainer::ACTION_ADDED:
				file << addedActionText;
				break;
			case CRevisionInfoContainer::ACTION_DELETED:
				file << deletedActionText;
				break;
			case CRevisionInfoContainer::ACTION_REPLACED:
				file << replacedActionText;
				break;
			default:
				break;
		}

		file << pathText;
		file << iter->GetPath().GetPath();

		file << pathEndText;
	}

	file << endText;
}

///////////////////////////////////////////////////////////////
// write <logentry> tag
///////////////////////////////////////////////////////////////

void CXMLLogWriter::WriteRevisionInfo ( CBufferedOutFile& file
									  , const CRevisionInfoContainer& info
									  , DWORD revision
									  , DWORD index)
{
	static const std::string startText = "<logentry\n   revision=\"";
	static const std::string revisionEndText = "\">\n";
	static const std::string startAuthorText = "<author>";
	static const std::string endAuthorText = "</author>\n";
	static const std::string msgText = "<msg>";
	static const std::string endText = "</msg>\n</logentry>\n";

	// <logentry> start tag (includes revision)

	file << startText;
	file << revision;
	file << revisionEndText;

	// write the author, if given

	const char* author = info.GetAuthor (index);
	if (*author != 0)
	{
		file << startAuthorText;
		file << author;
		file << endAuthorText;
	}

	// add time stamp, if given

	WriteTimeStamp (file, info.GetTimeStamp (index));

	// list the changed paths, if given

	WriteChanges (file, info.GetChangesBegin(index), info.GetChangesEnd(index));

	// always add the commit message

	file << msgText;
	file << info.GetComment (index);

	file << endText;
}

///////////////////////////////////////////////////////////////
// dump the revisions in descending order
///////////////////////////////////////////////////////////////

void CXMLLogWriter::WriteRevionsTopDown ( CBufferedOutFile& file
										, const CCachedLogInfo& source)
{
	const CRevisionIndex& revisions = source.GetRevisions();
	const CRevisionInfoContainer& info = source.GetLogInfo();

	for ( size_t revision = revisions.GetLastRevision()-1
		, fristRevision = revisions.GetFirstRevision()
		; revision+1 > fristRevision
		; --revision)
	{
		DWORD index = revisions[revision];
		if (index != -1)
		{
			WriteRevisionInfo (file, info, (DWORD)revision, index);
		}
	}
}

///////////////////////////////////////////////////////////////
// dump the revisions in ascending order
///////////////////////////////////////////////////////////////

void CXMLLogWriter::WriteRevionsBottomUp ( CBufferedOutFile& file
										 , const CCachedLogInfo& source)
{
	const CRevisionIndex& revisions = source.GetRevisions();
	const CRevisionInfoContainer& info = source.GetLogInfo();

	for ( size_t revision = revisions.GetFirstRevision()
		, lastRevision = revisions.GetLastRevision()
		; revision < lastRevision
		; ++revision)
	{
		DWORD index = revisions[revision];
		if (index != -1)
		{
			WriteRevisionInfo (file, info, (DWORD)revision, index);
		}
	}
}


///////////////////////////////////////////////////////////////
// write the whole change content
///////////////////////////////////////////////////////////////

void CXMLLogWriter::SaveToXML ( const std::wstring& xmlFileName
							  , const CCachedLogInfo& source
							  , bool topDown)
{
	CBufferedOutFile file (xmlFileName);

	const char* header = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<log>\n";
	const char* footer = "</log>\n";

	file << header;

	if (topDown)
	{
		WriteRevionsTopDown (file, source);
	}
	else
	{
		WriteRevionsBottomUp (file, source);
	}

	file << footer;
}

