#include "include/notepadqq.h"

const QString Notepadqq::version = POINTVERSION;
const QString Notepadqq::contributorsUrl = "https://github.com/notepadqq/notepadqq/blob/master/CONTRIBUTORS.md";

QString Notepadqq::copyright()
{
    return QObject::trUtf8("Copyright © 2010-2014, Daniele Di Sarli");
}

QString Notepadqq::editorPath()
{
    QString def = QString("%1/../appdata/editor/index.html").
            arg(qApp->applicationDirPath());

    if(!QFile(def).exists())
        def = QString("%1/../share/%2/editor/index.html").
                arg(qApp->applicationDirPath()).
                arg(qApp->applicationName().toLower());

    return def;
}