#include "stdafx.h"
#include "WinhttpUploader.h"

#include "ult/string-operate.h"
#include "ult/file-dir.h"

const std::string WinhttpUploader::kLineEnd_ = "\r\n";

WinhttpUploader::WinhttpUploader(void) {
  InitBoundary();
}


WinhttpUploader::~WinhttpUploader(void) {
}

void WinhttpUploader::InitBoundary(void) {
  //init boundary
  wboundary_ = L"----------";
  wboundary_ += ult::GetRandomString(L"", 30);
  aboundary_ = ult::UnicodeToAnsi(wboundary_);
}

void WinhttpUploader::Reset(void) {
  session_.Close();
  connection_.Close();
  this->Close();
  InitBoundary();
}

int WinhttpUploader::InitRequest(const std::wstring& url) {
  HRESULT hr = session_.Initialize(true);
  if (FAILED(hr)) {
    return ult::HttpStatus::kUnknownError;
  }
  URL_COMPONENTS uc;
  ult::UltWinHttpCrackUrl(url.c_str(), &uc);
  std::wstring host_name(uc.lpszHostName, uc.dwHostNameLength);
  hr = connection_.Initialize(session_.GetHandle(), host_name.c_str(), uc.nPort);
  if (FAILED(hr)) {
    return ult::HttpStatus::kConnectError;
  }
  hr = Initialize(connection_.GetHandle(), L"POST", uc.lpszUrlPath, uc.nScheme);
  if (FAILED(hr)) {
    return ult::HttpStatus::kOpenRequestError;
  }
  return ult::HttpStatus::kSuccess;
}

void WinhttpUploader::AddField(const std::wstring& field, const char* data, DWORD len) {
  if (len == 0) {
    return;
  }
  sendfield_ += "--" + aboundary_ + kLineEnd_ + "Content-Disposition: form-data; name=\"";
  sendfield_ += ult::UnicodeToAnsi(field) + "\"" + kLineEnd_ + kLineEnd_;
  sendfield_.append(data, len);
  sendfield_ += kLineEnd_;
}

int WinhttpUploader::BeginPost(const std::wstring& url, const std::wstring& filename,
                               DWORD sendsize, const std::wstring& field) {
  int ret = InitRequest(url);
  if (ret != ult::HttpStatus::kSuccess) {
    return ret;
  }
  std::wstring header = L"Content-Type: multipart/form-data; boundary=" + wboundary_;
  std::string postbegin;
  //biuld begin
  postbegin = "--" + aboundary_ + kLineEnd_ + "Content-Disposition: form-data; name=\"";
  postbegin += ult::UnicodeToAnsi(field) + "\"; filename=\"";
  postbegin += ult::UnicodeToAnsi(filename) + "\"" + kLineEnd_ + "Content-Type: application/octet-stream" + kLineEnd_ + kLineEnd_;
  //biuld end
  postend_ = kLineEnd_ + "--" + aboundary_ + "--" + kLineEnd_;
  size_t l1 = sendfield_.length();
  size_t l2 = postbegin.length();
  size_t l3 = postend_.length();
  UINT64 total_size = sendsize + sendfield_.length() + postbegin.length() + postend_.length();
  total_size = total_size > 0xffffffff ? 0xffffffff : total_size;
  SendRequest(header.c_str(), -1L, NULL, 0, (DWORD)total_size);
  
  if (sendfield_.length() > 0) {
    WriteData(sendfield_.c_str(), sendfield_.length());
  }
  WriteData(postbegin.c_str(), postbegin.length());
  return ult::HttpStatus::kSuccess;
}

HRESULT WinhttpUploader::PostFile(const void* data, DWORD len) {
  return WriteData(data, len);
}

HRESULT WinhttpUploader::EndPost(void) {
  HRESULT hr = WriteData(postend_.c_str(), postend_.length());
  RETURN_IF_FAILED(hr);
  return RecieveResponse();
}