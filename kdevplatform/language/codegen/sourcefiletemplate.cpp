/*
 * This file is part of KDevelop
 * Copyright 2012 Miha Čančula <miha@noughmad.eu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "sourcefiletemplate.h"
#include "templaterenderer.h"
#include <debug.h>

#include <interfaces/icore.h>

#include <KArchive>
#include <KConfig>
#include <KZip>
#include <KTar>
#include <KConfigGroup>

#include <QFileInfo>
#include <QDomDocument>
#include <QStandardPaths>
#include <QDir>

using namespace KDevelop;
typedef SourceFileTemplate::ConfigOption ConfigOption;

class KDevelop::SourceFileTemplatePrivate
{
public:
    KArchive* archive;
    QString descriptionFileName;
    QStringList searchLocations;

    ConfigOption readEntry(const QDomElement& element, TemplateRenderer* renderer);
};

ConfigOption SourceFileTemplatePrivate::readEntry(const QDomElement& element,
                                                  TemplateRenderer* renderer)
{
    ConfigOption entry;

    entry.name = element.attribute(QStringLiteral("name"));
    entry.type = element.attribute(QStringLiteral("type"), QStringLiteral("String"));

    bool isDefaultValueSet = false;
    for (QDomElement e = element.firstChildElement(); !e.isNull(); e = e.nextSiblingElement())
    {
        QString tag = e.tagName();

        if (tag == QLatin1String("label"))
        {
            entry.label = e.text();
        }
        else if (tag == QLatin1String("tooltip"))
        {
            entry.label = e.text();
        }
        else if (tag == QLatin1String("whatsthis"))
        {
            entry.label = e.text();
        }
        else if ( tag == QLatin1String("min") )
        {
            entry.minValue = e.text();
        }
        else if ( tag == QLatin1String("max") )
        {
            entry.maxValue = e.text();
        }
        else if ( tag == QLatin1String("default") )
        {
            entry.value = renderer->render(e.text(), entry.name);
            isDefaultValueSet = true;
        }
        else if (tag == QLatin1String("choices")) {
            QStringList values;
            QDomNodeList choices = element.elementsByTagName(QStringLiteral("choice"));
            values.reserve(choices.size());
            for (int j = 0; j < choices.size(); ++j) {
                QDomElement choiceElement = choices.at(j).toElement();
                values << choiceElement.attribute(QStringLiteral("name"));
            }
            Q_ASSERT(!values.isEmpty());
            if (values.isEmpty()) {
                qCWarning(LANGUAGE) << "Entry " << entry.name << "has an enum without any choices";
            }
            entry.values = values;
        }
    }

    qCDebug(LANGUAGE) << "Read entry" << entry.name << "with default value" << entry.value;

    // preset value for enum if needed
    if (!entry.values.isEmpty()) {
        if (isDefaultValueSet) {
            const bool isSaneDefaultValue = entry.values.contains(entry.value.toString());
            Q_ASSERT(isSaneDefaultValue);
            if (!isSaneDefaultValue) {
                qCWarning(LANGUAGE) << "Default value" << entry.value << "not in enum" << entry.values;
                entry.value = entry.values.at(0);
            }
        } else {
            entry.value = entry.values.at(0);
        }
    }
    return entry;
}


SourceFileTemplate::SourceFileTemplate (const QString& templateDescription)
: d(new KDevelop::SourceFileTemplatePrivate)
{
    d->archive = nullptr;
    setTemplateDescription(templateDescription);
}

SourceFileTemplate::SourceFileTemplate()
: d(new KDevelop::SourceFileTemplatePrivate)
{
    d->archive = nullptr;
}

SourceFileTemplate::SourceFileTemplate (const SourceFileTemplate& other)
: d(new KDevelop::SourceFileTemplatePrivate)
{
    d->archive = nullptr;
    *this = other;
}

SourceFileTemplate::~SourceFileTemplate()
{
    delete d->archive;
}

SourceFileTemplate& SourceFileTemplate::operator=(const SourceFileTemplate& other)
{
    if (other.d == d) {
        return *this;
    }

    delete d->archive;
    if (other.d->archive) {
        if (other.d->archive->fileName().endsWith(QLatin1String(".zip"))) {
            d->archive = new KZip(other.d->archive->fileName());
        } else {
            d->archive = new KTar(other.d->archive->fileName());
        }
        d->archive->open(QIODevice::ReadOnly);
    } else {
        d->archive = nullptr;
    }
    d->descriptionFileName = other.d->descriptionFileName;
    return *this;
}

void SourceFileTemplate::setTemplateDescription(const QString& templateDescription)
{
    delete d->archive;

    d->descriptionFileName = templateDescription;
    QString archiveFileName;

    const QString templateBaseName = QFileInfo(templateDescription).baseName();

    d->searchLocations.append(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("/kdevfiletemplates/templates/"), QStandardPaths::LocateDirectory));

    foreach(const QString& dir, d->searchLocations) {
        foreach(const auto& entry, QDir(dir).entryInfoList(QDir::Files)) {
            if (entry.baseName() == templateBaseName) {
                archiveFileName = entry.absoluteFilePath();
                qCDebug(LANGUAGE) << "Found template archive" << archiveFileName;
                break;
            }
        }
    }

    if (archiveFileName.isEmpty() || !QFileInfo::exists(archiveFileName)) {
        qCWarning(LANGUAGE) << "Could not find a template archive for description" << templateDescription << ", archive file" << archiveFileName;
        d->archive = nullptr;
    } else {
        QFileInfo info(archiveFileName);

        if (info.suffix() == QLatin1String("zip")) {
            d->archive = new KZip(archiveFileName);
        } else {
            d->archive = new KTar(archiveFileName);
        }
        d->archive->open(QIODevice::ReadOnly);
    }
}

bool SourceFileTemplate::isValid() const
{
    return d->archive;
}

QString SourceFileTemplate::name() const
{
    KConfig templateConfig(d->descriptionFileName);
    KConfigGroup cg(&templateConfig, "General");
    return cg.readEntry("Name");
}

QString SourceFileTemplate::type() const
{
    KConfig templateConfig(d->descriptionFileName);
    KConfigGroup cg(&templateConfig, "General");
    return cg.readEntry("Type", QString());
}

QString SourceFileTemplate::languageName() const
{
    KConfig templateConfig(d->descriptionFileName);
    KConfigGroup cg(&templateConfig, "General");
    return cg.readEntry("Language", QString());
}

QStringList SourceFileTemplate::category() const
{
    KConfig templateConfig(d->descriptionFileName);
    KConfigGroup cg(&templateConfig, "General");
    return cg.readEntry("Category", QStringList());
}

QStringList SourceFileTemplate::defaultBaseClasses() const
{
    KConfig templateConfig(d->descriptionFileName);
    KConfigGroup cg(&templateConfig, "General");
    return cg.readEntry("BaseClasses", QStringList());
}

const KArchiveDirectory* SourceFileTemplate::directory() const
{
    Q_ASSERT(isValid());
    return d->archive->directory();
}

QVector<SourceFileTemplate::OutputFile> SourceFileTemplate::outputFiles() const
{
    QVector<SourceFileTemplate::OutputFile> outputFiles;

    KConfig templateConfig(d->descriptionFileName);
    KConfigGroup group(&templateConfig, "General");

    const QStringList files = group.readEntry("Files", QStringList());
    qCDebug(LANGUAGE) << "Files in template" << files;

    outputFiles.reserve(files.size());
    for (const QString& fileGroup : files) {
        KConfigGroup cg(&templateConfig, fileGroup);
        OutputFile f;
        f.identifier = cg.name();
        f.label = cg.readEntry("Name");
        f.fileName = cg.readEntry("File");
        f.outputName = cg.readEntry("OutputFile");
        outputFiles << f;
    }

    return outputFiles;
}

bool SourceFileTemplate::hasCustomOptions() const
{
    Q_ASSERT(isValid());

    KConfig templateConfig(d->descriptionFileName);
    KConfigGroup cg(&templateConfig, "General");
    bool hasOptions = d->archive->directory()->entries().contains(cg.readEntry("OptionsFile", "options.kcfg"));

    qCDebug(LANGUAGE) << cg.readEntry("OptionsFile", "options.kcfg") << hasOptions;
    return hasOptions;
}

QVector<SourceFileTemplate::ConfigOptionGroup> SourceFileTemplate::customOptions(TemplateRenderer* renderer) const
{
    Q_ASSERT(isValid());

    KConfig templateConfig(d->descriptionFileName);
    KConfigGroup cg(&templateConfig, "General");
    const KArchiveEntry* entry = d->archive->directory()->entry(cg.readEntry("OptionsFile", "options.kcfg"));

    QVector<ConfigOptionGroup> optionGroups;

    if (!entry->isFile())
    {
        return optionGroups;
    }
    const KArchiveFile* file = static_cast<const KArchiveFile*>(entry);

    /*
     * Copied from kconfig_compiler.kcfg
     */
    QDomDocument doc;
    QString errorMsg;
    int errorRow;
    int errorCol;
    if ( !doc.setContent( file->data(), &errorMsg, &errorRow, &errorCol ) ) {
        qCDebug(LANGUAGE) << "Unable to load document.";
        qCDebug(LANGUAGE) << "Parse error in line " << errorRow << ", col " << errorCol << ": " << errorMsg;
        return optionGroups;
    }

    QDomElement cfgElement = doc.documentElement();
    if ( cfgElement.isNull() ) {
        qCDebug(LANGUAGE) << "No document in kcfg file";
        return optionGroups;
    }

    QDomNodeList groups = cfgElement.elementsByTagName(QStringLiteral("group"));
    optionGroups.reserve(groups.size());
    for (int i = 0; i < groups.size(); ++i)
    {
        QDomElement group = groups.at(i).toElement();
        ConfigOptionGroup optionGroup;
        optionGroup.name = group.attribute(QStringLiteral("name"));

        QDomNodeList entries = group.elementsByTagName(QStringLiteral("entry"));
        optionGroup.options.reserve(entries.size());
        for (int j = 0; j < entries.size(); ++j)
        {
            QDomElement entry = entries.at(j).toElement();
            optionGroup.options << d->readEntry(entry, renderer);
        }

        optionGroups << optionGroup;
    }
    return optionGroups;
}

void SourceFileTemplate::addAdditionalSearchLocation(const QString& location)
{
    if(!d->searchLocations.contains(location))
        d->searchLocations.append(location);
}
