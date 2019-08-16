/*
	Copyright 2009-2012, Sumeet Chhetri 
  
    Licensed under the Apache License, Version 2.0 (the "License"); 
    you may not use this file except in compliance with the License. 
    You may obtain a copy of the License at 
  
        http://www.apache.org/licenses/LICENSE-2.0 
  
    Unless required by applicable law or agreed to in writing, software 
    distributed under the License is distributed on an "AS IS" BASIS, 
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
    See the License for the specific language governing permissions and 
    limitations under the License.  
*/
/*
 * HttpResponse.cpp
 *
 *  Created on: Aug 19, 2009
 *      Author: sumeet
 */

#include "HttpResponse.h"

std::string_view HttpResponse::VALID_RESPONSE_HEADERS = ",access-control-allow-origin,access-control-allow-headers,access-control-allow-credentials,access-control-allow-methods,access-control-max-age,accept-ranges,age,allow,cache-control,connection,content-encoding,content-language,content-length,content-location,content-md5,content-disposition,content-range,content-type,date,etag,expires,last-modified,link,location,p3p,pragma,proxy-authenticate,refresh,retry-after,server,set-cookie,status,strict-transport-security,trailer,transfer-encoding,vary,via,warning,www-authenticate,";
std::string_view HttpResponse::AccessControlAllowOrigin		 = "Access-Control-Allow-Origin";
std::string_view HttpResponse::AccessControlAllowHeaders		 = "Access-Control-Allow-Headers";
std::string_view HttpResponse::AccessControlAllowCredentials	 = "Access-Control-Allow-Credentials";
std::string_view HttpResponse::AccessControlAllowMethods		 = "Access-Control-Allow-Methods";
std::string_view HttpResponse::AccessControlMaxAge		 = "Access-Control-Max-Age";
std::string_view HttpResponse::AcceptRanges			 = "Accept-Ranges";
std::string_view HttpResponse::Age				 = "Age";
std::string_view HttpResponse::Allow				 = "Allow";
std::string_view HttpResponse::CacheControl			 = "Cache-Control";
std::string_view HttpResponse::Connection				 = "Connection";
std::string_view HttpResponse::ContentEncoding			 = "Content-Encoding";
std::string_view HttpResponse::ContentLanguage			 = "Content-Language";
std::string_view HttpResponse::ContentLength			 = "Content-Length";
std::string_view HttpResponse::ContentLocation			 = "Content-Location";
std::string_view HttpResponse::ContentMD5				 = "Content-MD5";
std::string_view HttpResponse::ContentDisposition			 = "Content-Disposition";
std::string_view HttpResponse::ContentRange			 = "Content-Range";
std::string_view HttpResponse::ContentType			 = "Content-Type";
std::string_view HttpResponse::DateHeader				 = "Date";
std::string_view HttpResponse::ETag				 = "ETag";
std::string_view HttpResponse::Expires				 = "Expires";
std::string_view HttpResponse::LastModified			 = "Last-Modified";
std::string_view HttpResponse::Link				 = "Link";
std::string_view HttpResponse::Location				 = "Location";
std::string_view HttpResponse::P3P				 = "P3P";
std::string_view HttpResponse::Pragma				 = "Pragma";
std::string_view HttpResponse::ProxyAuthenticate			 = "Proxy-Authenticate";
std::string_view HttpResponse::Refresh				 = "Refresh";
std::string_view HttpResponse::RetryAfter				 = "Retry-After";
std::string_view HttpResponse::Server				 = "Server";
std::string_view HttpResponse::SetCookie				 = "Set-Cookie";
std::string_view HttpResponse::Status				 = "Status";
std::string_view HttpResponse::StrictTransportSecurity		 = "Strict-Transport-Security";
std::string_view HttpResponse::Trailer				 = "Trailer";
std::string_view HttpResponse::TransferEncoding			 = "Transfer-Encoding";
std::string_view HttpResponse::Vary				 = "Vary";
std::string_view HttpResponse::Via				 = "Via";
std::string_view HttpResponse::Warning				 = "Warning";
std::string_view HttpResponse::WWWAuthenticate			 = "WWW-Authenticate";
std::string_view HttpResponse::Upgrade = 			 "Upgrade";
std::string_view HttpResponse::SecWebSocketAccept = "Sec-WebSocket-Accept";
std::string_view HttpResponse::SecWebSocketVersion = "Sec-WebSocket-Version";
std::string_view HttpResponse::AltSvc = "Alt-Svc";

HttpResponse::HttpResponse() {
	httpVersion = "HTTP/1.1";
	compressed = false;
	tecurrpart = 0;
	teparts = 0;
	techunkSiz = 0;
	hasContent = false;
	intCntLen = -1;
	httpVers = 0;
	done = false;
	statusCode = -1;
}

HttpResponse::~HttpResponse() {
}

std::string_view HttpResponse::generateResponse(std::string_view httpMethod, HttpRequest *req, const bool& appendHeaders /*= true*/)
{
	if(httpMethod=="HEAD" && appendHeaders)
	{
		return generateHeadResponse();
	}
	else if(httpMethod=="OPTIONS" && appendHeaders)
	{
		return generateOptionsResponse();
	}
	else if(httpMethod=="TRACE" && appendHeaders)
	{
		return generateTraceResponse(req);
	}
	else
	{
		if(appendHeaders)
		{
			return generateHeadResponse() + this->content;
		}
		else
		{
			return generateHeadResponse();
		}
	}
}

std::string_view HttpResponse::generateResponse(HttpRequest *req, const bool& appendHeaders /*= true*/)
{
	if(req->getMethod()=="OPTIONS")
	{
		return generateOptionsResponse();
	}
	else if(req->getMethod()=="TRACE")
	{
		return generateTraceResponse(req);
	}
	else
	{
		if(appendHeaders)
		{
			return generateHeadResponse() + this->content;
		}
		else
		{
			return generateHeadResponse();
		}
	}
}

std::string_view HttpResponse::generateResponse(const bool& appendHeaders /*= true*/)
{
	if(appendHeaders)
	{
		return generateHeadResponse() + this->content;
	}
	else
	{
		generateHeadResponse();
		return this->content;
	}
}

const std::string_view HttpResponse::HDR_SRV = "Server: FFEAD 2.0\r\n";
const std::string_view HttpResponse::HDR_SEPT = ":";
const std::string_view HttpResponse::HDR_SEP = ": ";
const std::string_view HttpResponse::HDR_END = "\r\n";
const std::string_view HttpResponse::HDR_FIN = "\r\n\r\n";

std::string_view HttpResponse::generateHeadResponse()
{
	bool isTE = isHeaderValue(TransferEncoding, "chunked");
	std::string_view resp, boundary;
	if(this->contentList.size()>0)
	{
		content.clear();
		boundary = "FFEAD_SERVER_" + CastUtil::lexical_cast<std::string>(Timer::getCurrentTime());
		for (int var = 0; var < (int)contentList.size(); ++var) {
			content += "--" + boundary + HDR_END;
			RMap headers = contentList.at(var).getHeaders();
			RMap::iterator it;
			for(it=headers.begin();it!=headers.end();++it)
			{
				content += it->first + HDR_SEP + it->second + HDR_END;
			}
			content += HDR_END;
			content += contentList.at(var).getContent();
			content += HDR_END;
		}
		content += "--" + boundary + "--" + HDR_END;
	}
	resp = (httpVersion + " " + statusCode + " " + statusMsg + HDR_END);
	resp += HDR_SRV;
	if(this->getHeader(ContentType)=="" && this->contentList.size()>0)
	{
		this->addHeader(ContentType, "multipart/mixed");
	}
	if(this->getHeader(ContentType)!="" && boundary!="")
	{
		headers[ContentType] += "; boundary=\"" + boundary + "\"";
	}
	if(!isTE && getHeader(ContentLength)=="")
	{
		addHeader(ContentLength, CastUtil::lexical_cast<std::string>((int)content.length()));
	}
	RMap::iterator it;
	for(it=headers.begin();it!=headers.end();++it)
	{
		resp += it->first + HDR_SEP + it->second + HDR_END;
	}
	for (int var = 0; var < (int)this->cookies.size(); var++)
	{
		resp += SetCookie + HDR_SEP + this->cookies.at(var) + HDR_END;
	}
	resp += HDR_END;
	return resp;
}

const std::string_view HttpResponse::HDR_CORS_ALW = "Allow: OPTIONS, GET, HEAD, POST, PUT, DELETE, TRACE\r\n";

std::string_view HttpResponse::generateOptionsResponse()
{
	std::string_view resp;
	resp = (httpVersion + " " + statusCode + " " + statusMsg + HDR_END);
	resp += HDR_SRV;
	RMap::iterator it;
	for(it=headers.begin();it!=headers.end();++it)
	{
		resp += it->first + HDR_SEP + it->second + HDR_END;
	}
	for (int var = 0; var < (int)this->cookies.size(); var++)
	{
		resp += SetCookie + HDR_SEP + this->cookies.at(var) + HDR_END;
	}
	resp += HDR_CORS_ALW;
	resp += HDR_END;
	return resp;
}

std::string_view HttpResponse::generateTraceResponse(HttpRequest* req)
{
	std::string_view resp;
	resp = (httpVersion + " " + statusCode + " " + statusMsg + HDR_END);
	resp += HDR_SRV;
	RMap::iterator it;
	for(it=headers.begin();it!=headers.end();++it)
	{
		resp += it->first + HDR_SEP + it->second + HDR_END;
	}
	for (int var = 0; var < (int)this->cookies.size(); var++)
	{
		resp += SetCookie + HDR_SEP + this->cookies.at(var) + HDR_END;
	}
	resp += HDR_END;
	if(req!=NULL)
	{
		resp += "TRACE " + req->getActUrl() + " " + req->getHttpVersion();
		resp += HDR_END;
		RMap::iterator it;
		for(it=headers.begin();it!=headers.end();++it)
		{
			resp += it->first + HDR_SEP + it->second + HDR_END;
		}
	}
	return resp;
}


std::string_view HttpResponse::getHttpVersion() const
{
	return httpVersion;
}

void HttpResponse::update(HttpRequest* req)
{
	this->httpVers = req->httpVers;
	this->httpVersion = req->getHttpVersion();
	addHeader(HttpResponse::AcceptRanges, "none");
}

void HttpResponse::setHTTPResponseStatus(const HTTPResponseStatus& status)
{
	this->statusCode = status.getSCode();
	this->statusMsg = status.getMsg();
}

std::string_view HttpResponse::getStatusCode() const
{
	return statusCode;
}

void HttpResponse::setStatusCode(std::string_view statusCode)
{
	this->statusCode = statusCode;
}

std::string_view HttpResponse::getStatusMsg() const
{
	return statusMsg;
}

void HttpResponse::setStatusMsg(std::string_view statusMsg)
{
	this->statusMsg = statusMsg;
}

std::string_view HttpResponse::getContent() const
{
	return content;
}

void HttpResponse::setContent(std::string_view content)
{
	this->content = content;
	if(content!="") {
		hasContent = true;
	}
}

void HttpResponse::addCookie(std::string_view cookie)
{
	this->cookies.push_back(cookie);
}

void HttpResponse::addContent(const MultipartContent& content)
{
	contentList.push_back(content);
}

void HttpResponse::addHeader(std::string_view header, std::string_view value)
{
	if(headers.find(header)!=headers.end()) {
		headers[header] += "," + value;
	} else {
		headers[header] = value;
	}
}

void HttpResponse::addHeaderValue(std::string_view header, std::string_view value)
{
	if(header!="")
	{
		if(VALID_RESPONSE_HEADERS.find(","+header+",")!=std::string::npos)
		{
			if(headers.find(header)!=headers.end()) {
				headers[header] += "," + value;
			} else {
				headers[header] = value;
			}
		}
		else
		{
			//std::cout << ("Non standard Header string " + header) << std::endl;
			std::vector<std::string> matres = RegexUtil::search(header, "^[a-zA-Z]+[-|a-zA-Z]+[a-zA-Z]*[a-zA-Z]$");
			if(matres.size()==0)
			{
				//std::cout << ("Invalid Header string " + header) << std::endl;
				return;
			}
			if(headers.find(header)!=headers.end()) {
				headers[header] += "," + value;
			} else {
				headers[header] = value;
			}
		}
	}
}

bool HttpResponse::isHeaderValue(std::string_view header, std::string_view value, const bool& ignoreCase)
{
	return header!="" && headers.find(header)!=headers.end()
			&& (headers[header]==value || (ignoreCase && strcasecmp(&headers[header][0], &value[0])==0));
}

std::string_view HttpResponse::getHeader(std::string_view header)
{
	if(header!="" && headers.find(header)!=headers.end())
		return headers[header];
	return "";
}

bool HttpResponse::isNonBinary()
{
	std::string_view mimeType = getHeader(ContentType);
	return (strcasestr(&mimeType[0], "text")!=NULL || strcasestr(&mimeType[0], "css")!=NULL
			|| strcasestr(&mimeType[0], "x-javascript")!=NULL || strcasestr(&mimeType[0], "json")!=NULL
			|| strcasestr(&mimeType[0], "xml")!=NULL || strcasestr(&mimeType[0], "html")!=NULL);
}

void HttpResponse::setCompressed(const bool& compressed)
{
	this->compressed = compressed;
}

bool HttpResponse::getCompressed()
{
	return this->compressed;
}

const std::vector<std::string> HttpResponse::getCookies() const {
	return cookies;
}

std::string_view HttpResponse::getStatusLine() const {
	return (httpVersion + " " + statusCode + " " + statusMsg);
}

unsigned int HttpResponse::getContentSize(const char *fileName)
{
	if((int)intCntLen!=-1) {
		return intCntLen;
	}
	unsigned int siz = 0;
	if(fileName!=NULL)
	{
		std::ifstream myfile;
		myfile.open(fileName, std::ios::binary | std::ios::ate);
		if (myfile.is_open())
		{
			myfile.seekg(0, std::ios::end);
			siz = myfile.tellg();
			myfile.close();
		}
	}
	else
	{
		siz = content.length();
	}
	intCntLen = siz;
	return siz;
}

std::string_view HttpResponse::getContent(const char *fileName, const int& start, const int& end)
{
	std::string_view all;
	if(fileName!=NULL)
	{
		std::ifstream myfile;
		myfile.open(fileName, std::ios::in | std::ios::binary);
		if (myfile.is_open())
		{
			if(start==-1 && end==-1)
			{
				std::string content((std::istreambuf_iterator<char>(myfile)), (std::istreambuf_iterator<char>()));
				all = content;
			}
			else
			{
				myfile.seekg(start);
				std::string s(end, '\0');
				myfile.read(&s[0], end);
				all = s;
			}
			myfile.close();
		}
	}
	else
	{
		if(start==-1 && end==-1)
		{
			all = content;
		}
		else
		{
			all = content.substr(start, end-start);
		}
	}
	return all;
}

bool HttpResponse::updateContent(HttpRequest* req, const uint32_t& techunkSiz)
{
	hasContent = false;
	this->httpVersion = req->getHttpVersion();
	this->httpVers = req->httpVers;

	std::string_view ext = req->getExt();

	HttpResponse *res = this;
	std::vector<std::string_view> rangesVec;
	std::vector<std::vector<int> > rangeValuesLst = req->getRanges(rangesVec);

	std::string_view fname = req->getUrl();
	std::string_view locale = CommonUtils::getLocale(req->getDefaultLocale());
	std::string_view type = CommonUtils::getMimeType(ext);

	std::string_view all;
	if (fname=="/")
    {
		res->setHTTPResponseStatus(HTTPResponseStatus::NotFound);
		return false;
    }

	/*ifstream myfile;
    if(locale.find("english")==std::string::npos && (ext==".html" || ext==".htm"))
    {
    	std::string_view tfname = fname;
    	StringUtil::replaceFirst(tfname, "." , ("_" + locale+"."));
    	std::ifstream gzipdfile(tfname.c_str(), std::ios::binary);
		if(gzipdfile.good())
		{
			fname = tfname;
			gzipdfile.close();
		}
    }*/

	if(req->getMethod()=="HEAD")
	{
		res->addHeader(HttpResponse::ContentLength, CastUtil::lexical_cast<std::string>(getContentSize(&fname[0])));
		res->addHeader(HttpResponse::AcceptRanges, "bytes");
		res->setHTTPResponseStatus(HTTPResponseStatus::Ok);
		res->addHeader(HttpResponse::ContentType, CommonUtils::getMimeType(ext));
	}
	else if(req->getMethod()=="OPTIONS" || req->getMethod()=="TRACE")
	{
		res->setHTTPResponseStatus(HTTPResponseStatus::Ok);
	}
	else
	{
		bool isCEGzip = isHeaderValue("Content-Encoding", "gzip");
		bool isCEDef = isHeaderValue("Content-Encoding", "deflate");
		if(this->contentList.size()==0 && res->content=="")
		{
			struct tm tim;
			struct stat attrib;
			if(stat(&fname[0], &attrib)!=0)
			{
				res->setHTTPResponseStatus(HTTPResponseStatus::NotFound);
				return false;
			}
			//as per suggestion at http://stackoverflow.com/questions/10446526/get-last-modified-time-of-file-in-linux
			gmtime_r(&(attrib.st_mtime), &tim);


			Date filemodifieddate(&tim);

			bool isifmodsincvalid = false;

			std::string_view ifmodsincehdr = req->getHeader(HttpRequest::IfModifiedSince);

			bool forceLoadFile = false;
			if(ifmodsincehdr!="")
			{
				Date* ifmodsince = NULL;
				try {
					DateFormat df("%a, %d %b %Y %H:%M:%S GMT");
					ifmodsince = df.parse(ifmodsincehdr);
					isifmodsincvalid = true;
					//std::cout << "Parsed date success" << std::endl;
				} catch(const std::exception& e) {
					isifmodsincvalid = false;
				}

				if(ifmodsince!=NULL)
				{
					//std::cout << "IfModifiedSince header = " + ifmodsincehdr + ", date = " + ifmodsince->toString() << std::endl;
					//std::cout << "Lastmodifieddate value = " + lastmodDate + ", date = " + filemodifieddate.toString() << std::endl;
					//std::cout << "Date Comparisons = " +CastUtil::lexical_cast<std::string>(*ifmodsince>=filemodifieddate)  << std::endl;

					if(isifmodsincvalid && *ifmodsince>=filemodifieddate)
					{
						res->addHeader(HttpResponse::LastModified, ifmodsincehdr);
						//std::cout << ("File not modified - IfModifiedSince date = " + ifmodsincehdr + ", FileModified date = " + lastmodDate) << std::endl;
						res->setHTTPResponseStatus(HTTPResponseStatus::NotModified);
						return false;
					}
					else if(isifmodsincvalid && *ifmodsince<filemodifieddate)
					{
						//std::cout << ("File modified - IfModifiedSince date = " + ifmodsincehdr + ", FileModified date = " + lastmodDate) << std::endl;
						forceLoadFile = true;
					}
					delete ifmodsince;
				}
			}

			time_t rt;
			struct tm ti;
			time (&rt);
			gmtime_r(&rt, &ti);
			char buffer[31];
			strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &ti);
			res->addHeader(HttpResponse::LastModified, std::string(buffer));

			if(isCEGzip)
			{
				bool gengzipfile = true;
				std::string ofname = std::string(req->getContextHome());
				ofname += "/temp/" + req->getFile() + ".gz";
				if(!forceLoadFile)
				{
					std::ifstream gzipdfile(ofname.c_str(), std::ios::binary);
					if(gzipdfile.good())
					{
						gzipdfile.close();
						gengzipfile = false;
					}
				}
				if(gengzipfile)
				{
					CompressionUtil::gzipCompressFile((char*)&fname[0], false, (char*)ofname.c_str());
				}
				fname = ofname;
				res->setCompressed(true);
				req->setUrl(fname);
			}
			else if(isCEDef)
			{
				bool genzlibfile = true;
				std::string ofname = std::string(req->getContextHome());
				ofname += "/temp/" + req->getFile() + ".z";
				if(!forceLoadFile)
				{
					std::ifstream gzipdfile(ofname.c_str(), std::ios::binary);
					if(gzipdfile.good())
					{
						gzipdfile.close();
						genzlibfile = false;
					}
				}
				if(genzlibfile)
				{
					CompressionUtil::zlibCompressFile((char*)&fname[0], false, (char*)ofname.c_str());
				}
				fname = ofname;
				res->setCompressed(true);
				req->setUrl(fname);
			}

			if(req->getHttpVers()<1.1 && rangeValuesLst.size()>0)
			{
				res->setHTTPResponseStatus(HTTPResponseStatus::InvalidReqRange);
				return false;
			}
			else if(rangeValuesLst.size()>0)
			{
				res->setHTTPResponseStatus(HTTPResponseStatus::PartialContent);
				res->addHeader(HttpResponse::ContentType, "multipart/byteranges");
				unsigned int totlen = getContentSize(&fname[0]);
				res->addHeader(HttpResponse::ContentLength, CastUtil::lexical_cast<std::string>(totlen));
				for (int var = 0; var <(int)rangeValuesLst.size(); ++var) {
					int start = rangeValuesLst.at(var).at(0);
					int end = rangeValuesLst.at(var).at(1);
					if(end!=-1 && end>=(int)totlen && start!=-1 && start<(int)totlen)
					{
						//error
					}
					else
					{
						if(start==-1)
							start = 0;
						if(end==-1)
							end = totlen - 1;
						else
							end += 1;
						std::string_view cont = getContent(&fname[0], start, end);
						MultipartContent conte(cont);
						conte.addHeader(MultipartContent::ContentType, type);
						conte.addHeader(HttpResponse::ContentRange, "bytes "+rangesVec.at(var)+"/"+CastUtil::lexical_cast<std::string>(totlen));
						res->addContent(conte);
					}
				}
			}
		}
		else if(this->contentList.size()==0)
		{
			if(isCEGzip)
			{
				this->content = CompressionUtil::gzipCompress(this->content, true);
			}
			if(isCEDef)
			{
				this->content = CompressionUtil::zlibCompress(this->content, true);
			}
		}

		unsigned int totlen = getContentSize(&fname[0]);
		if(techunkSiz>0 && techunkSiz<totlen)
		{
			res->techunkSiz = techunkSiz;
			float parts = techunkSiz!=0?(float)totlen/techunkSiz:0;
			parts = (floor(parts)<parts?floor(parts)+1:floor(parts));
			res->teparts = (int)parts;
			res->content = "";
			if(res->httpVers>=1.1 && res->httpVers<1.2) {
				res->addHeader(HttpResponse::TransferEncoding, "chunked");
			} else {
				res->addHeader(ContentLength, CastUtil::lexical_cast<std::string>(totlen));
			}
		}
		else
		{
			res->content = getContent(&fname[0]);
			res->addHeader(ContentLength, CastUtil::lexical_cast<std::string>((int)res->content.length()));
		}
		res->setHTTPResponseStatus(HTTPResponseStatus::Ok);
		res->addHeader(HttpResponse::ContentType, CommonUtils::getMimeType(ext));
		hasContent = true;
	}
	return hasContent;
}

std::string_view HttpResponse::getRemainingContent(std::string_view fname, const bool& isFirst) {
	std::string_view rem;
	if(isContentRemains() && httpVers>=1.1) {
		unsigned int totlen = getContentSize(&fname[0]);
		unsigned int len = totlen - techunkSiz*tecurrpart;
		if((int)len>techunkSiz)
		{
			len = techunkSiz;
		}
		if(httpVers<2.0)
		{
			rem = StringUtil::toHEX(len) + "\r\n";
			rem += getContent(&fname[0], techunkSiz*tecurrpart, len);
			rem += "\r\n";
			if(tecurrpart+1==teparts) {
				rem += "0\r\n\r\n";
			}
		}
		else
		{
			rem = getContent(&fname[0], techunkSiz*tecurrpart, len);
		}
		tecurrpart++;
	} else if(isFirst || (httpVers>=1.0 && httpVers<1.1)) {
		rem = content;
	}
	return rem;
}

bool HttpResponse::isContentRemains() {
	return teparts>0 && tecurrpart<teparts;
}

std::string_view HttpResponse::toPluginString() {
	std::string_view text = (this->statusCode + "\n");
	text += (this->statusMsg + "\n");
	text += (CastUtil::lexical_cast<std::string>(this->httpVersion) + "\n");
	text += (this->outFileName + "\n");

	text += (CastUtil::lexical_cast<std::string>(this->content.length()) + "\n");
	text += (this->content);

	text += (CastUtil::lexical_cast<std::string>(this->preamble.length()) + "\n");
	text += (this->preamble);

	text += (CastUtil::lexical_cast<std::string>(this->epilogue.length()) + "\n");
	text += (this->epilogue);

	std::map<std::string,std::string,cicomp>::iterator it;
	text += (CastUtil::lexical_cast<std::string>(this->headers.size()) + "\n");
	for(it=this->headers.begin();it!=this->headers.end();++it)
	{
		text += it->first + "\n";
		text += CastUtil::lexical_cast<std::string>(it->second.length()) + "\n";
		text += it->second;
	}

	text += (CastUtil::lexical_cast<std::string>(this->multipartFormData.size()) + "\n");
	FMap::iterator fit;
	for(fit=this->multipartFormData.begin();fit!=this->multipartFormData.end();++fit)
	{
		text += fit->second.name + "\n";
		text += fit->second.fileName + "\n";
		text += fit->second.tempFileName + "\n";
		text += (CastUtil::lexical_cast<std::string>(fit->second.content.length()) + "\n");
		text += (fit->second.content);
		text += (CastUtil::lexical_cast<std::string>(fit->second.headers.size()) + "\n");
		for(it=fit->second.headers.begin();it!=fit->second.headers.end();++it)
		{
			text += it->first + "\n";
			text += CastUtil::lexical_cast<std::string>(it->second.length()) + "\n";
			text += it->second;
		}
	}

	text += (CastUtil::lexical_cast<std::string>(this->contentList.size()) + "\n");
	for(int k=0;k<(int)this->contentList.size();k++)
	{
		text += this->contentList.at(k).name + "\n";
		text += this->contentList.at(k).fileName + "\n";
		text += this->contentList.at(k).tempFileName + "\n";
		text += (CastUtil::lexical_cast<std::string>(this->contentList.at(k).content.length()) + "\n");
		text += (this->contentList.at(k).content);
		text += (CastUtil::lexical_cast<std::string>(this->contentList.at(k).headers.size()) + "\n");
		for(it=this->contentList.at(k).headers.begin();it!=this->contentList.at(k).headers.end();++it)
		{
			text += it->first + "\n";
			text += CastUtil::lexical_cast<std::string>(it->second.length()) + "\n";
			text += it->second;
		}
	}

	return text;
}

bool HttpResponse::isDone() const {
	return done;
}

void HttpResponse::setDone(const bool& done) {
	this->done = done;
}

std::string_view HttpResponse::getFileExtension(std::string_view file)
{
	if(file.find_last_of(".")!=std::string::npos)return file.substr(file.find_last_of("."));
	return file;
}

const RMap& HttpResponse::getCHeaders() const {
	return headers;
}
RMap HttpResponse::getHeaders() const {
	return headers;
}
