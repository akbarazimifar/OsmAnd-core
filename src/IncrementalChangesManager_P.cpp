#include "IncrementalChangesManager_P.h"
#include "IncrementalChangesManager.h"

#include "QtCommon.h"
#include "ignore_warnings_on_external_includes.h"
#include <QXmlStreamReader>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QBuffer>
#include "restore_internal_warnings.h"

#include "OsmAndCore_private.h"
#include "CoreResourcesEmbeddedBundle.h"
#include "ObfReader.h"
#include "ArchiveReader.h"
#include "ObfDataInterface.h"
#include "ResolvedMapStyle.h"
#include "UnresolvedMapStyle.h"
#include "OnlineTileSources.h"
#include "QKeyValueIterator.h"
#include "Logging.h"
#include "Utilities.h"
#include "CachedOsmandIndexes.h"
#include "ResourcesManager.h"

OsmAnd::IncrementalChangesManager_P::IncrementalChangesManager_P(
    IncrementalChangesManager* owner_,
    const std::shared_ptr<const IWebClient>& webClient_,
    ResourcesManager* resourcesManager_)
    : owner(owner_)
    , _webClient(webClient_)
    , _resourcesManager(resourcesManager_)
{
}

OsmAnd::IncrementalChangesManager_P::~IncrementalChangesManager_P()
{
}

void OsmAnd::IncrementalChangesManager_P::initialize()
{
}

bool OsmAnd::IncrementalChangesManager_P::addValidIncrementalUpdates(QHash< QString, std::shared_ptr<const ResourcesManager::LocalResource> > &liveResources,
                                                                     QHash< QString, std::shared_ptr<const ResourcesManager::LocalResource> > &mapResources)
{
    QHash< QString, std::shared_ptr<const ResourcesManager::LocalResource> > result(mapResources);
    QHash<QString, uint64_t> regionMaps;
    for (const auto &res : mapResources)
    {
        if (!res || res->origin != ResourcesManager::ResourceOrigin::Installed)
            continue;
        
        const auto& installedResource = std::static_pointer_cast<const ResourcesManager::InstalledResource>(res);
        const uint64_t timestamp = installedResource->timestamp;
        QString regionName = QString(installedResource->id).remove(QStringLiteral(".map.obf"));
        regionMaps.insert(regionName, timestamp);
        _updatesStructure.insert(regionName, std::shared_ptr<RegionUpdateFiles>(new RegionUpdateFiles(regionName, installedResource)));
    }
    
    for (const auto &liveRes : liveResources)
    {
        if (!liveRes || liveRes->origin != ResourcesManager::ResourceOrigin::Installed)
            continue;
        
        const auto& liveResource = std::static_pointer_cast<const ResourcesManager::InstalledResource>(liveRes);
        QString regionName = QString(liveResource->id).remove(QRegExp(QStringLiteral("_([0-9]+_){2}[0-9]+\\.live\\.obf")));
        
        if (regionMaps.contains(regionName))
        {
            if (liveResource->timestamp > regionMaps.value(regionName))
            {
                mapResources.insert(liveRes->id, qMove(liveRes));
                _updatesStructure.value(regionName)->addUpdate(liveResource);
            }
            else
                _resourcesManager->uninstallResource(liveResource, liveRes);
        }
    }
    
    return true;
}

bool OsmAnd::IncrementalChangesManager_P::getIncrementalUpdatesForRegion(QString &region,
                                                                         long timestamp,
                                                                         QList< std::shared_ptr<const IncrementalUpdate> >& resources) const
{
    std::shared_ptr<const IWebClient::IRequestResult> requestResult;
    const auto& downloadResult = _webClient->downloadData(
                                                          owner->repositoryBaseUrl +
                                                          QStringLiteral("check_live?aosmc=true&timestamp=") +
                                                          QString::number(timestamp) +
                                                          "&file=" + QUrl::toPercentEncoding(region),
                                                          &requestResult);
    if (downloadResult.isNull() || !requestResult->isSuccessful())
        return false;
    // Parse XML
    QXmlStreamReader xmlReader(downloadResult);
    bool ok = parseRepository(xmlReader, resources);
    if (!ok)
        return false;
    
    
    return true;
}

bool OsmAnd::IncrementalChangesManager_P::parseRepository(
                                                 QXmlStreamReader& xmlReader,
                                                 QList< std::shared_ptr<const IncrementalUpdate> >& repository) const
{
    bool ok = false;
    while (!xmlReader.atEnd() && !xmlReader.hasError())
    {
        xmlReader.readNext();
        if (!xmlReader.isStartElement())
            continue;
        const auto tagName = xmlReader.name();
        const auto& attribs = xmlReader.attributes();
        
        if (tagName.isNull() || tagName != QStringLiteral("update"))
            continue;
        
        const auto& dateValue = attribs.value(QStringLiteral("updateDate"));
        if (dateValue.isNull())
            continue;
        const auto& containerSizeValue = attribs.value(QStringLiteral("containerSize"));
        if (containerSizeValue.isNull())
            continue;
        const auto& contentSizeValue = attribs.value(QStringLiteral("contentSize"));
        if (contentSizeValue.isNull())
            continue;
        const auto& timestampValue = attribs.value(QStringLiteral("timestamp"));
        if (timestampValue.isNull())
            continue;
        const auto& sizeTextValue = attribs.value(QStringLiteral("size"));
        if (sizeTextValue.isNull())
            continue;
        const auto& nameValue = attribs.value(QStringLiteral("name"));
        if (nameValue.isNull())
            continue;
        
        const auto name = nameValue.toString();
        
        
        const auto timestamp = timestampValue.toULongLong(&ok);
        if (!ok)
        {
            LogPrintf(LogSeverityLevel::Warning,
                      "Invalid timestamp '%s' for '%s'",
                      qPrintableRef(timestampValue),
                      qPrintable(name));
            continue;
        }
        
        const auto containerSize = containerSizeValue.toULongLong(&ok);
        if (!ok)
        {
            LogPrintf(LogSeverityLevel::Warning,
                      "Invalid container size '%s' for '%s'",
                      qPrintableRef(containerSizeValue),
                      qPrintable(name));
            continue;
        }
        
        const auto contentSize = contentSizeValue.toULongLong(&ok);
        if (!ok)
        {
            LogPrintf(LogSeverityLevel::Warning,
                      "Invalid content size '%s' for '%s'",
                      qPrintableRef(contentSizeValue),
                      qPrintable(name));
            continue;
        }
        QString resourceId = QString(name)
        .remove(QLatin1String(".obf.gz"))
        .toLower()
        .append(QLatin1String(".live.obf"));
        
        QString urlString = owner->repositoryBaseUrl +
        QLatin1String("download.php?file=") +
        QUrl::toPercentEncoding(name) +
        QStringLiteral("&aosmc=yes");
        
        IncrementalUpdate update;
        update.fileName = name;
        update.containerSize = containerSize;
        update.contentSize = contentSize;
        update.sizeText = sizeTextValue.toString();
        update.date = dateValue.toString();
        update.timestamp = timestamp;
        update.resId = resourceId;
        update.url = QUrl(urlString);
        
        repository.append(std::make_shared<const IncrementalUpdate>(update));
    }
    
    return true;
}

std::shared_ptr<const OsmAnd::IncrementalChangesManager_P::IncrementalUpdateList> OsmAnd::IncrementalChangesManager_P::getUpdatesByMonth(QString &regionName) const
{
    IncrementalUpdateList updateList;
    std::shared_ptr<RegionUpdateFiles> ruf = _updatesStructure.value(regionName);
    updateList.updateFiles = ruf;
//    if(ruf->isEmpty()) {
//        updateList.errorMessage = QStringLiteral("No installed updates for this region");
//        return std::make_shared<const IncrementalUpdateList>(updateList);
//    }
    long timestamp = ruf->getTimestamp();
    QList< std::shared_ptr<const IncrementalUpdate> > resources;
    getIncrementalUpdatesForRegion(regionName, timestamp, resources);
    for(const auto& res : resources)
        updateList.addUpdate(res);
    
    return std::make_shared<const IncrementalUpdateList>(updateList);
}
