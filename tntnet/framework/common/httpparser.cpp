/* httpparser.cpp
 * Copyright (C) 2003-2005 Tommi Maekitalo
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <tnt/httpparser.h>
#include <tnt/httperror.h>
#include <tnt/httpheader.h>
#include <cxxtools/log.h>
#include <sstream>
#include <algorithm>

#define SET_STATE(new_state)  state = &Parser::new_state

namespace tnt
{
  namespace
  {
    std::string chartoprint(char ch)
    {
      const static char hex[] = "0123456789abcdef";
      if (std::isprint(ch))
        return std::string(1, '\'') + ch + '\'';
      else
        return std::string("'\\x") + hex[(ch >> 4) & 0xf] + hex[ch & 0xf] + '\'';
    }

    inline bool istokenchar(char ch)
    {
      static const char s[] = "\"(),/:;<=>?@[\\]{}";
      return std::isalpha(ch) || std::binary_search(s, s + sizeof(s) - 1, ch);
    }

    inline bool isHexDigit(char ch)
    {
      return ch >= '0' && ch <= '9'
          || ch >= 'A' && ch <= 'Z'
          || ch >= 'a' && ch <= 'z';
    }

    inline unsigned valueOfHexDigit(char ch)
    {
      return ch >= '0' && ch <= '9' ? ch - '0'
           : ch >= 'a' && ch <= 'z' ? ch - 'a' + 10
           : ch >= 'A' && ch <= 'Z' ? ch - 'A' + 10
           : 0;
    }
  }

  log_define("tntnet.httpmessage.parser")

  bool RequestSizeMonitor::post(bool ret)
  {
    if (++requestSize > HttpMessage::getMaxRequestSize()
      && HttpMessage::getMaxRequestSize() > 0)
    {
      requestSizeExceeded();
      return true;
    }
    return ret;
  }

  void RequestSizeMonitor::requestSizeExceeded()
  { }

  void HttpMessage::Parser::reset()
  {
    message.clear();
    SET_STATE(state_cmd0);
    httpCode = HTTP_OK;
    failedFlag = false;
    RequestSizeMonitor::reset();
    headerParser.reset();
  }

  bool HttpMessage::Parser::state_cmd0(char ch)
  {
    if (istokenchar(ch))
    {
      message.method.clear();
      message.method.reserve(16);
      message.method += ch;
      SET_STATE(state_cmd);
    }
    else if (ch != ' ' && ch != '\t')
    {
      log_warn("invalid character " << chartoprint(ch) << " in method");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }
    return failedFlag;
  }

  bool HttpMessage::Parser::state_cmd(char ch)
  {
    if (istokenchar(ch))
      message.method += ch;
    else if (ch == ' ')
    {
      log_debug("method=" << message.method);
      SET_STATE(state_url0);
    }
    else
    {
      log_warn("invalid character " << chartoprint(ch) << " in method");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }
    return failedFlag;
  }

  bool HttpMessage::Parser::state_url0(char ch)
  {
    if (ch == ' ' || ch == '\t')
    {
    }
    else if (ch > ' ')
    {
      message.url.clear();
      message.url.reserve(32);
      message.url += ch;
      SET_STATE(state_url);
    }
    else
    {
      log_warn("invalid character " << chartoprint(ch) << " in url");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }

    return failedFlag;
  }

  bool HttpMessage::Parser::state_url(char ch)
  {
    if (ch == '?')
    {
      log_debug("url=" << message.url);
      SET_STATE(state_qparam);
    }
    else if (ch == '\r')
    {
      log_debug("url=" << message.url);
      SET_STATE(state_end0);
    }
    else if (ch == '\n')
    {
      log_debug("url=" << message.url);
      SET_STATE(state_header);
    }
    else if (ch == ' ' || ch == '\t')
    {
      log_debug("url=" << message.url);
      SET_STATE(state_version);
    }
    else if (ch == '+')
      message.url += ' ';
    else if (ch == '%')
    {
      SET_STATE(state_urlesc);
      message.url += ch;
    }
    else if (ch > ' ')
      message.url += ch;
    else
    {
      log_warn("invalid character " << chartoprint(ch) << " in url");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }
    return failedFlag;
  }

  bool HttpMessage::Parser::state_urlesc(char ch)
  {
    if (isHexDigit(ch))
    {
      if (message.url.size() >= 2 && message.url[message.url.size() - 2] == '%')
      {
        unsigned v = (valueOfHexDigit(message.url[message.url.size() - 1]) << 4) | valueOfHexDigit(ch);
        message.url[message.url.size() - 2] = static_cast<char>(v);
        message.url.resize(message.url.size() - 1);
        SET_STATE(state_url);
      }
      else
      {
        message.url += ch;
      }
      return false;
    }
    else
    {
      SET_STATE(state_url);
      return state_url(ch);
    }
  }

  bool HttpMessage::Parser::state_qparam(char ch)
  {
    if (ch == ' ' || ch == '\t')
    {
      log_debug("queryString=" << message.queryString);
      SET_STATE(state_version);
    }
    else
      message.queryString += ch;
    return false;
  }

  bool HttpMessage::Parser::state_version(char ch)
  {
    if (ch == '/')
    {
      message.majorVersion = 0;
      message.minorVersion = 0;
      skipWs(&Parser::state_version_major);
    }
    else if (ch == '\r')
    {
      log_warn("invalid character " << chartoprint(ch) << " in version");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }
    return failedFlag;
  }

  bool HttpMessage::Parser::state_version_major(char ch)
  {
    if (ch == '.')
      SET_STATE(state_version_minor0);
    else if (std::isdigit(ch))
      message.majorVersion = message.majorVersion * 10 + (ch - '0');
    else if (ch == ' ' || ch == '\t')
      SET_STATE(state_version_major_sp);
    else
    {
      log_warn("invalid character " << chartoprint(ch) << " in version-major");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }
    return failedFlag;
  }

  bool HttpMessage::Parser::state_version_major_sp(char ch)
  {
    if (ch == '.')
      SET_STATE(state_version_minor0);
    else
    {
      log_warn("invalid character " << chartoprint(ch) << " in version-major");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }
    return failedFlag;
  }

  bool HttpMessage::Parser::state_version_minor0(char ch)
  {
    return ch == ' ' || ch == '\t' ? failedFlag
                                   : state_version_minor(ch);
  }

  bool HttpMessage::Parser::state_version_minor(char ch)
  {
    if (ch == '\n')
      SET_STATE(state_header);
    else if (ch == ' ' || ch == '\t' || ch == '\r')
      SET_STATE(state_end0);
    else if (std::isdigit(ch))
      message.minorVersion = message.minorVersion * 10 + (ch - '0');
    else
    {
      log_warn("invalid character " << chartoprint(ch) << " in version-minor");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }
    return failedFlag;
  }

  bool HttpMessage::Parser::state_end0(char ch)
  {
    if (ch == '\n')
      SET_STATE(state_header);
    else if (ch != ' ' && ch != '\t')
    {
      log_warn("invalid character " << chartoprint(ch) << " in end");
      httpCode = HTTP_BAD_REQUEST;
      failedFlag = true;
    }
    return failedFlag;
  }

  bool HttpMessage::Parser::state_header(char ch)
  {
    if (headerParser.parse(ch))
    {
      if (headerParser.failed())
      {
        httpCode = HTTP_BAD_REQUEST;
        failedFlag = true;
        return true;
      }

      std::string content_length_header = message.getHeader(httpheader::contentLength);
      if (!content_length_header.empty())
      {
        std::istringstream valuestream(content_length_header);
        valuestream >> bodySize;
        if (!valuestream)
          throw HttpError("400 missing Content-Length");

        if (getMaxRequestSize() > 0
          && getCurrentRequestSize() + bodySize > getMaxRequestSize())
        {
          requestSizeExceeded();
          return true;
        }

        message.contentSize = bodySize;
        if (bodySize == 0)
          return true;
        else
        {
          SET_STATE(state_body);
          message.body.reserve(bodySize);
          return false;
        }
      }

      return true;
    }

    return false;
  }

  bool HttpMessage::Parser::state_body(char ch)
  {
    message.body += ch;
    return --bodySize == 0;
  }

  void HttpMessage::Parser::requestSizeExceeded()
  {
    log_warn("max request size " << getMaxRequestSize() << " exceeded");
    httpCode = HTTP_REQUEST_ENTITY_TOO_LARGE;
    failedFlag = true;
  }
}
