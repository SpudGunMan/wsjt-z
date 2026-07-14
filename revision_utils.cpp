#include "revision_utils.hpp"

#include <cstring>

#include <QCoreApplication>
#include <QRegularExpression>

#include "scs_version.h"

namespace
{
  QString revision_extract_number (QString const& s)
  {
    QString revision;

    // try and match a number (hexadecimal allowed)
    QRegularExpression re {R"(^[$:]\w+: (r?[\da-f]+[^$]*)\$$)"};
    auto match = re.match (s);
    if (match.hasMatch ())
      {
        revision = match.captured (1);
      }
    return revision;
  }
}

QString revision (QString const& scs_rev_string)
{
  QString result {SCS_VERSION_STR};
  if (result.isEmpty ())
    {
      auto revision_from_scs = revision_extract_number (scs_rev_string);
      if (!revision_from_scs.isEmpty ())
        result = revision_from_scs;
      else
        result = "dev";  // Fallback when no SCS/git is available
    }
  return result;
}

QString version (bool include_patch)
{
#if defined (CMAKE_BUILD)
  QString v {TO_STRING__ (PROJECT_VERSION_MAJOR) "." TO_STRING__ (PROJECT_VERSION_MINOR)};
  if (include_patch)
    {
      v += "." TO_STRING__ (PROJECT_VERSION_PATCH) + QString {BUILD_TYPE_REVISION};
    }
#else
  QString v {"Not for Release"};
#endif
  return v;
}

QString program_title (QString const& revision)
{
  QString id {QCoreApplication::applicationName () + "   v" + QCoreApplication::applicationVersion ()};
  return id + "  " + revision + " ";
}
