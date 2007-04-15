/* unzipcomp.cpp
 * Copyright (C) 2003 Tommi Maekitalo
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "static.h"
#include "unzip.h"

#include <tnt/component.h>
#include <tnt/componentfactory.h>
#include <tnt/httprequest.h>
#include <tnt/httpreply.h>
#include <tnt/http.h>
#include <tnt/unzipfile.h>
#include <cxxtools/log.h>

log_define("tntnet.unzip")

namespace tnt
{
  class MimeHandler;

  ////////////////////////////////////////////////////////////////////////
  // componentdeclaration
  //
  class Unzip : public Static
  {
      friend class UnzipFactory;

    public:
      virtual unsigned operator() (tnt::HttpRequest& request,
        tnt::HttpReply& reply, cxxtools::QueryParams& qparam);
      virtual void drop();
  };

  ////////////////////////////////////////////////////////////////////////
  // factory
  //
  class UnzipFactory : public StaticFactory
  {
    public:
      UnzipFactory(const std::string& componentName)
        : StaticFactory(componentName)
        { }
      virtual tnt::Component* doCreate(const tnt::Compident& ci,
        const tnt::Urlmapper& um, tnt::Comploader& cl);
  };

  tnt::Component* UnzipFactory::doCreate(const tnt::Compident&,
    const tnt::Urlmapper&, tnt::Comploader&)
  {
    return new Unzip();
  }

  TNT_COMPONENTFACTORY(unzip, UnzipFactory)

  ////////////////////////////////////////////////////////////////////////
  // componentdefinition
  //

  unsigned Unzip::operator() (tnt::HttpRequest& request,
    tnt::HttpReply& reply, cxxtools::QueryParams& qparams)
  {
    std::string pi = request.getPathInfo();

    if (request.getArgsCount() < 1)
      reply.throwError(HTTP_INTERNAL_SERVER_ERROR, "missing archive name");

    log_debug("unzip archive \"" << request.getArg(0) << "\" file \"" << pi << '"');

    try
    {
      unzipFile f(request.getArg(0));
      unzipFileStream in(f, pi, false);

      // set Content-Type
      if (request.getArgs().size() > 1 && request.getArg(1).size() > 0)
        reply.setContentType(request.getArg(1));
      else
        setContentType(request, reply);

      reply.out() << in.rdbuf();
    }
    catch (const unzipEndOfListOfFile&)
    {
      log_debug("file \"" << pi << "\" not found in archive");
      return DECLINED;
    }

    return HTTP_OK;
  }

  void Unzip::drop()
  {
    factory.drop(this);
  }

}
