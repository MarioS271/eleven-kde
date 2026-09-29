// Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
// SPDX-License-Identifier: LGPL-3.0-only

#include <QtWidgets/QStylePlugin>
#include "elevenstyle.h"

class ElevenStylePlugin : public QStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QStyleFactoryInterface_iid FILE "eleven-kde.json")
public:
    QStyle *create(const QString &key) override
    {
        if (key.compare(QLatin1String("eleven-kde"), Qt::CaseInsensitive) == 0)
            return new ElevenStyle;
        return nullptr;
    }
};

#include "main.moc"
