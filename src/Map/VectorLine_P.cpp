#include "VectorLine_P.h"
#include "VectorLine.h"
#include "Utilities.h"

#include "ignore_warnings_on_external_includes.h"
#include "restore_internal_warnings.h"

#include <SkBitmap.h>
#include <SkPathMeasure.h>

#include "MapSymbol.h"
#include "MapSymbolsGroup.h"
#include "VectorMapSymbol.h"
#include "OnSurfaceVectorMapSymbol.h"
#include "OnSurfaceRasterMapSymbol.h"
#include "QKeyValueIterator.h"
#include "Logging.h"
#include "AtlasMapRenderer.h"
#include "SkiaUtilities.h"
#include "GeometryModifiers.h"

#include <Polyline2D/Polyline2D.h>
#include <Polyline2D/Vec2.h>

#define SPECIAL_ARROW_DISTANCE_MULTIPLIER 2.5f

// Colorization shemes
#define COLORIZATION_NONE 0
#define COLORIZATION_GRADIENT 1
#define COLORIZATION_SOLID 2


// The smaller delta, the less line is simplified and the more time it takes to generate primitives
#define MIN_ALPHA_DELTA 0.1f
#define MIN_RGB_DELTA 0.075f

OsmAnd::VectorLine_P::VectorLine_P(VectorLine* const owner_)
    : _hasUnappliedChanges(false)
    , _hasUnappliedPrimitiveChanges(false)
    , _isHidden(false)
    , _isApproximationEnabled(true)
    , _colorizationSceme(0)
    , _lineWidth(1.0)
    , _outlineWidth(0.0)
    , _pathIconStep(-1.0f)
    , _specialPathIconStep(-1.0f)
    , _metersPerPixel(1.0)
    , _mapZoomLevel(InvalidZoomLevel)
    , _mapVisualZoom(0.f)
    , _mapVisualZoomShift(0.f)
    , owner(owner_)
{
}

OsmAnd::VectorLine_P::~VectorLine_P()
{
}

bool OsmAnd::VectorLine_P::isHidden() const
{
    QReadLocker scopedLocker(&_lock);

    return _isHidden;
}

void OsmAnd::VectorLine_P::setIsHidden(const bool hidden)
{
    QWriteLocker scopedLocker(&_lock);

    if (_isHidden != hidden)
    {
        _isHidden = hidden;
        _hasUnappliedChanges = true;
    }
}

bool OsmAnd::VectorLine_P::showArrows() const
{
    QReadLocker scopedLocker(&_lock);

    return _showArrows;
}

void OsmAnd::VectorLine_P::setShowArrows(const bool showArrows)
{
    QWriteLocker scopedLocker(&_lock);

    if (_showArrows != showArrows)
    {
        _showArrows = showArrows;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

bool OsmAnd::VectorLine_P::isApproximationEnabled() const
{
    QReadLocker scopedLocker(&_lock);

    return _isApproximationEnabled;
}

void OsmAnd::VectorLine_P::setApproximationEnabled(const bool enabled)
{
    QWriteLocker scopedLocker(&_lock);

    if (_isApproximationEnabled != enabled)
    {
        _isApproximationEnabled = enabled;
        _hasUnappliedChanges = true;
    }
}

QVector<OsmAnd::PointI> OsmAnd::VectorLine_P::getPoints() const
{
    QReadLocker scopedLocker(&_lock);

    return _points;
}

void OsmAnd::VectorLine_P::setPoints(const QVector<PointI>& points)
{
    QWriteLocker scopedLocker(&_lock);

    if (_points != points)
    {
        _points = points;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

QList<OsmAnd::FColorARGB> OsmAnd::VectorLine_P::getColorizationMapping() const
{
    QReadLocker scopedLocker(&_lock);

    return _colorizationMapping;
}

void OsmAnd::VectorLine_P::setColorizationMapping(const QList<FColorARGB> &colorizationMapping)
{
    QWriteLocker scopedLocker(&_lock);

    if (_colorizationMapping != colorizationMapping)
    {
        _colorizationMapping = colorizationMapping;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

bool OsmAnd::VectorLine_P::hasColorizationMapping() const
{
    return _colorizationMapping.size() == _points.size();
}

double OsmAnd::VectorLine_P::getLineWidth() const
{
    QReadLocker scopedLocker(&_lock);

    return _lineWidth;
}

void OsmAnd::VectorLine_P::setLineWidth(const double width)
{
    QWriteLocker scopedLocker(&_lock);

    if (_lineWidth != width)
    {
        _lineWidth = width;

        if (owner->pathIcon)
        {
            double newWidth = _lineWidth / 3.4f;
            double scale = newWidth / owner->pathIcon->width();
            auto scaledPathIcon = SkiaUtilities::scaleImage(owner->pathIcon, scale, scale);
            _scaledPathIcon = scaledPathIcon ? scaledPathIcon : owner->pathIcon;
        }
        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }

}

float OsmAnd::VectorLine_P::getPathIconStep() const
{
    QReadLocker scopedLocker(&_lock);

    return _pathIconStep;
}

void OsmAnd::VectorLine_P::setPathIconStep(const float step)
{
    QWriteLocker scopedLocker(&_lock);

    if (!qFuzzyCompare(_pathIconStep, step))
    {
        _pathIconStep = step;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

float OsmAnd::VectorLine_P::getSpecialPathIconStep() const
{
    QReadLocker scopedLocker(&_lock);

    return _specialPathIconStep;
}

void OsmAnd::VectorLine_P::setSpecialPathIconStep(const float step)
{
    QWriteLocker scopedLocker(&_lock);

    if (!qFuzzyCompare(_specialPathIconStep, step))
    {
        _specialPathIconStep = step;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

double OsmAnd::VectorLine_P::getOutlineWidth() const
{
    QReadLocker scopedLocker(&_lock);

    return _outlineWidth;
}

void OsmAnd::VectorLine_P::setOutlineWidth(const double width)
{
    QWriteLocker scopedLocker(&_lock);

    if (_outlineWidth != width)
    {
        _outlineWidth = width;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

OsmAnd::FColorARGB OsmAnd::VectorLine_P::getOutlineColor() const
{
    QReadLocker scopedLocker(&_lock);

    return _outlineColor;
}

void OsmAnd::VectorLine_P::setOutlineColor(const FColorARGB color)
{
    QWriteLocker scopedLocker(&_lock);

    if (_outlineColor != color)
    {
        _outlineColor = color;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

void OsmAnd::VectorLine_P::setColorizationScheme(const int colorizationScheme)
{
    QWriteLocker scopedLocker(&_lock);

    if (_colorizationSceme != colorizationScheme)
    {
        _colorizationSceme = colorizationScheme;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

OsmAnd::FColorARGB OsmAnd::VectorLine_P::getFillColor() const
{
    QReadLocker scopedLocker(&_lock);

    return _fillColor;
}

void OsmAnd::VectorLine_P::setFillColor(const FColorARGB color)
{
    QWriteLocker scopedLocker(&_lock);

    if (_fillColor != color)
    {
        _fillColor = color;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

std::vector<double> OsmAnd::VectorLine_P::getLineDash() const
{
    QReadLocker scopedLocker(&_lock);

    return _dashPattern;
}

void OsmAnd::VectorLine_P::setLineDash(const std::vector<double> dashPattern)
{
    QWriteLocker scopedLocker(&_lock);

    if (_dashPattern != dashPattern)
    {
        _dashPattern = dashPattern;

        _hasUnappliedPrimitiveChanges = true;
        _hasUnappliedChanges = true;
    }
}

bool OsmAnd::VectorLine_P::hasUnappliedChanges() const
{
    QReadLocker scopedLocker(&_lock);

    return _hasUnappliedChanges;
}

bool OsmAnd::VectorLine_P::hasUnappliedPrimitiveChanges() const
{
    QReadLocker scopedLocker(&_lock);

    return _hasUnappliedPrimitiveChanges;
}

bool OsmAnd::VectorLine_P::isMapStateChanged(const MapState& mapState) const
{
    bool changed = qAbs(_mapZoomLevel + _mapVisualZoom - mapState.zoomLevel - mapState.visualZoom) > 0.1;
    changed |= _hasElevationDataProvider != mapState.hasElevationDataProvider;
    if (!changed && _visibleBBoxShifted != mapState.visibleBBoxShifted)
    {
        const AreaI64 visibleBBoxShifted(_visibleBBoxShifted);
        auto bboxShiftPoint = visibleBBoxShifted.topLeft - mapState.visibleBBoxShifted.topLeft;
        bool bboxChanged = abs(bboxShiftPoint.x) > visibleBBoxShifted.width()
            || abs(bboxShiftPoint.y) > visibleBBoxShifted.height();
        changed |= bboxChanged;
    }

    //_mapZoomLevel != mapState.zoomLevel ||
    //_mapVisualZoom != mapState.visualZoom ||
    //_mapVisualZoomShift != mapState.visualZoomShift;
    return changed;
}

void OsmAnd::VectorLine_P::applyMapState(const MapState& mapState)
{
    _metersPerPixel = mapState.metersPerPixel;
    _visibleBBoxShifted = mapState.visibleBBoxShifted;
    _mapZoomLevel = mapState.zoomLevel;
    _mapVisualZoom = mapState.visualZoom;
    _mapVisualZoomShift = mapState.visualZoomShift;
    _hasElevationDataProvider = mapState.hasElevationDataProvider;
}

bool OsmAnd::VectorLine_P::update(const MapState& mapState)
{
    QWriteLocker scopedLocker(&_lock);

    bool mapStateChanged = isMapStateChanged(mapState);
    if (mapStateChanged)
        applyMapState(mapState);

    _hasUnappliedPrimitiveChanges |= mapStateChanged;
    return mapStateChanged;
}

bool OsmAnd::VectorLine_P::applyChanges()
{
    QReadLocker scopedLocker1(&_lock);

    if (!_hasUnappliedChanges && !_hasUnappliedPrimitiveChanges)
        return false;

    QReadLocker scopedLocker2(&_symbolsGroupsRegistryLock);
    for (const auto& symbolGroup_ : constOf(_symbolsGroupsRegistry))
    {
        const auto symbolGroup = symbolGroup_.lock();
        if (!symbolGroup)
            continue;

        //const auto& vectorLineSymbolGroup = std::dynamic_pointer_cast<VectorLine::SymbolsGroup>(symbolGroup);
        bool needUpdatePrimitive = _hasUnappliedPrimitiveChanges && _points.size() > 1;
        for (const auto& symbol_ : constOf(symbolGroup->symbols))
        {
            symbol_->isHidden = _isHidden;

            if (const auto symbol = std::dynamic_pointer_cast<OnSurfaceVectorMapSymbol>(symbol_))
            {
                if (needUpdatePrimitive)
                    generatePrimitive(symbol);
            }
        }
    }
    owner->updatedObservable.postNotify(owner);
    _hasUnappliedChanges = false;
    _hasUnappliedPrimitiveChanges = false;

    return true;
}

std::shared_ptr<OsmAnd::VectorLine::SymbolsGroup> OsmAnd::VectorLine_P::inflateSymbolsGroup()
{
    QReadLocker scopedLocker(&_lock);

    // Construct new map symbols group for this marker
    const std::shared_ptr<VectorLine::SymbolsGroup> symbolsGroup(new VectorLine::SymbolsGroup(std::const_pointer_cast<VectorLine_P>(shared_from_this())));
    symbolsGroup->presentationMode |= MapSymbolsGroup::PresentationModeFlag::ShowAllOrNothing;

    if (_points.size() > 1)
    {
        const auto& vectorLine = std::make_shared<OnSurfaceVectorMapSymbol>(symbolsGroup);
        generatePrimitive(vectorLine);
        vectorLine->allowFastCheckByFrustum = false;
        symbolsGroup->symbols.push_back(vectorLine);
        owner->updatedObservable.postNotify(owner);
    }

    return symbolsGroup;
}

std::shared_ptr<OsmAnd::VectorLine::SymbolsGroup> OsmAnd::VectorLine_P::createSymbolsGroup(const MapState& mapState)
{
    applyMapState(mapState);

    const auto inflatedSymbolsGroup = inflateSymbolsGroup();
    registerSymbolsGroup(inflatedSymbolsGroup);
    return inflatedSymbolsGroup;
}

void OsmAnd::VectorLine_P::registerSymbolsGroup(const std::shared_ptr<MapSymbolsGroup>& symbolsGroup) const
{
    QWriteLocker scopedLocker(&_symbolsGroupsRegistryLock);

    _symbolsGroupsRegistry.insert(symbolsGroup.get(), symbolsGroup);
}

void OsmAnd::VectorLine_P::unregisterSymbolsGroup(MapSymbolsGroup* const symbolsGroup) const
{
    QWriteLocker scopedLocker(&_symbolsGroupsRegistryLock);

    _symbolsGroupsRegistry.remove(symbolsGroup);
}

OsmAnd::PointD OsmAnd::VectorLine_P::findLineIntersection(PointD p1, OsmAnd::PointD p2, OsmAnd::PointD p3, OsmAnd::PointD p4) const
{
    double d = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
    //double atn1 = atan2(p1.x - p2.x, p1.y - p2.y);
    //double atn2 = atan2(p3.x - p4.x, p3.y - p4.y);
    //double df = qAbs(atn1 - atn2);
    // printf("\n %f %f d=%f df=%f df-PI=%f df-2PI=%f", atn1, atn2, d, df, df - M_PI, df - 2 * M_PI);
    //double THRESHOLD = M_PI / 6;
    if(d == 0 // || df < THRESHOLD  || qAbs(df - M_PI) < THRESHOLD || qAbs(df - 2 * M_PI) < THRESHOLD
       ) {
        // in case of lines connecting p2 == p3
        return p2;
    }
    OsmAnd::PointD r;
    r.x = ((p1.x* p2.y-p1.y*p2.x)*(p3.x - p4.x) - (p3.x* p4.y-p3.y*p4.x)*(p1.x - p2.x)) / d;
    r.y = ((p1.x* p2.y-p1.y*p2.x)*(p3.y - p4.y) - (p3.x* p4.y-p3.y*p4.x)*(p1.y - p2.y)) / d;

    return r;
}

OsmAnd::PointD OsmAnd::VectorLine_P::getProjection(PointD point, PointD from, PointD to ) const
{
    double mDist = (from.x - to.x) * (from.x - to.x) + (from.y - to.y) * (from.y - to.y);
    double projection = scalarMultiplication(from.x, from.y, to.x, to.y, point.x, point.y);
    if (projection < 0)
    {
        return from;
    }
    else if (projection >= mDist)
    {
        return to;
    }
    return PointD(from.x + (to.x - from.x) * (projection / mDist),
                  from.y + (to.y - from.y) * (projection / mDist));
}

double OsmAnd::VectorLine_P::scalarMultiplication(double xA, double yA, double xB, double yB, double xC, double yC) const
{
    // Scalar multiplication between (AB, AC)
    return (xB - xA) * (xC - xA) + (yB - yA) * (yC - yA);
}

int OsmAnd::VectorLine_P::simplifyDouglasPeucker(
    const std::vector<PointI>& points,
    const uint start,
    const uint end,
    const double epsilon,
    std::vector<bool>& include) const
{
    double dmax = -1;
    int index = -1;
    for (int i = start + 1; i <= end - 1; i++)
    {
        PointD pointToProject = static_cast<PointD>(points[i]);
        PointD startPoint = static_cast<PointD>(points[start]);
        PointD endPoint = static_cast<PointD>(points[end]);
        PointD proj = getProjection(pointToProject, startPoint, endPoint);
        double d = qSqrt((points[i].x - proj.x) * (points[i].x - proj.x) +
                         (points[i].y - proj.y) * (points[i].y - proj.y));
        // calculate distance from line
        if (d > dmax)
        {
            dmax = d;
            index = i;
        }
    }
    if (dmax >= epsilon && index > -1)
    {
        int enabled1 = simplifyDouglasPeucker(points, start, index, epsilon, include);
        int enabled2 = simplifyDouglasPeucker(points, index, end, epsilon, include);
        return enabled1 + enabled2;
    }
    else
    {
        include[end] = true;
        return 1;
    }
}

bool OsmAnd::VectorLine_P::forceIncludePoint(const QList<FColorARGB>& pointsColors, const uint pointIndex) const
{
    if (!hasColorizationMapping())
        return false;

    const auto currColor = pointsColors[pointIndex];

    const auto pPrevColor = pointIndex == 0 ? nullptr : &pointsColors[pointIndex - 1];
    const auto pNextColor = pointIndex + 1 == pointsColors.size() ? nullptr : &pointsColors[pointIndex + 1];

    if (_colorizationSceme == COLORIZATION_SOLID)
        return !pPrevColor || *pPrevColor != currColor;

    if (_colorizationSceme == COLORIZATION_GRADIENT)
    {
        bool highColorDiff = false;
        highColorDiff |= pPrevColor && qAbs(pPrevColor->a - currColor.a) > MIN_ALPHA_DELTA;
        highColorDiff |= pNextColor && qAbs(pNextColor->a - currColor.a) > MIN_ALPHA_DELTA;
        highColorDiff |= pPrevColor && currColor.getRGBDelta(*pPrevColor) > MIN_RGB_DELTA;
        highColorDiff |= pNextColor && currColor.getRGBDelta(*pNextColor) > MIN_RGB_DELTA;
        return highColorDiff;
    }

    return false;
}

OsmAnd::FColorARGB OsmAnd::VectorLine_P::middleColor(
    const FColorARGB& first, const FColorARGB& last, const float factor) const
{
    return FColorARGB(
        first.a + (last.a - first.a) * factor,
        first.r + (last.r - first.r) * factor,
        first.g + (last.g - first.g) * factor,
        first.b + (last.b - first.b) * factor);
}

void OsmAnd::VectorLine_P::calculateVisibleSegments(std::vector<std::vector<PointI>>& segments, QList<QList<FColorARGB>>& segmentColors) const
{
    // Use enlarged visible area
    const AreaI64 visibleBBox64(_visibleBBoxShifted);
    auto visibleArea64 = visibleBBox64.getEnlargedBy(PointI64(visibleBBox64.width(), visibleBBox64.height()));
    visibleArea64.topLeft.x = visibleArea64.topLeft.x < INT32_MIN ? INT32_MIN : visibleArea64.topLeft.x;
    visibleArea64.topLeft.y = visibleArea64.topLeft.y < INT32_MIN ? INT32_MIN : visibleArea64.topLeft.y;
    visibleArea64.bottomRight.x = visibleArea64.bottomRight.x > INT32_MAX ? INT32_MAX : visibleArea64.bottomRight.x;
    visibleArea64.bottomRight.y = visibleArea64.bottomRight.y > INT32_MAX ? INT32_MAX : visibleArea64.bottomRight.y;
    const AreaI visibleArea(visibleArea64);

    // Calculate points unwrapped
    auto pointsCount = _points.size();
    int64_t intFull = INT32_MAX;
    intFull++;
    const auto intHalf = static_cast<int32_t>(intFull >> 1);
    const PointI shiftToCenter(intHalf, intHalf);
    auto point31 = _points[0];
    PointI64 point64(point31 - shiftToCenter);
    QVector<PointI64> points64;
    points64.reserve(pointsCount);
    points64.push_back(point64);
    QVector<int> pointIndices(pointsCount);
    pointIndices[0] = 0;
    int nextIndex = 0;
    AreaI64 bbox(point64, point64);
    PointI nextPoint31;
    auto pointsTotal = 1;
    for (int i = 1; i < pointsCount; i++)
    {
        auto offset = _points[i] - point31;
        if (offset.x >= intHalf)
            offset.x = offset.x - INT32_MAX - 1;
        else if (offset.x < -intHalf)
            offset.x = offset.x + INT32_MAX + 1;
        nextPoint31 = Utilities::normalizeCoordinates(PointI64(point31) + offset, ZoomLevel31);
        Utilities::calculateShortestPath(point64, point31, nextPoint31, bbox.topLeft, bbox.bottomRight, &points64);
        point64 += offset;
        points64.push_back(point64);
        bbox.enlargeToInclude(point64);
        auto pointsSize = points64.size();
        nextIndex += pointsSize - pointsTotal;
        pointIndices[i] = nextIndex;
        pointsTotal = pointsSize;
        point31 = nextPoint31;
    }
    auto minShiftX = static_cast<int32_t>(bbox.topLeft.x / intFull - (bbox.topLeft.x % intFull < 0 ? 1 : 0));
    auto minShiftY = static_cast<int32_t>(bbox.topLeft.y / intFull - (bbox.topLeft.y % intFull < 0 ? 1 : 0));
    auto maxShiftX = static_cast<int32_t>(bbox.bottomRight.x / intFull + (bbox.bottomRight.x % intFull < 0 ? 0 : 1));
    auto maxShiftY = static_cast<int32_t>(bbox.bottomRight.y / intFull + (bbox.bottomRight.y % intFull < 0 ? 0 : 1));

    // Use full map shifts to collect all visible segments
    const auto withColors = hasColorizationMapping();
    pointsCount = points64.size();
    PointI64 curr, drawFrom, drawTo, inter1, inter2;
    FColorARGB colorFrom, colorTo, colorSubFrom, colorSubTo, colorInterFrom, colorInterTo;
    std::vector<PointI> segment;
    QList<FColorARGB> colors;
    for (int shiftX = minShiftX; shiftX <= maxShiftX; shiftX++)
    {
        for (int shiftY = minShiftY; shiftY <= maxShiftY; shiftY++)
        {
            const PointI64 shift(shiftX * intFull, shiftY * intFull);
            bool segmentStarted = false;
            if (withColors)
            {
                colorTo = _colorizationMapping[0];
                colorSubTo = _colorizationMapping[0];
            }
            auto prev = drawFrom = points64[0] - shift;
            int prevIndex;
            nextIndex = 0;            
            int j = 0;
            bool prevIn = visibleArea64.contains(prev);
            for (int i = 1; i < pointsCount; i++)
            {
                curr = drawTo = points64[i] - shift;
                if (withColors)
                {
                    if (i > nextIndex)
                    {
                        prevIndex = nextIndex;
                        nextIndex = pointIndices[++j];
                        colorFrom = colorTo;
                        colorTo = _colorizationMapping[j];
                    }
                    colorSubFrom = colorSubTo;
                    const auto factor = static_cast<float>(i - prevIndex) / static_cast<float>(nextIndex - prevIndex);
                    colorSubTo = middleColor(colorFrom, colorTo, factor);
                }
                bool currIn = visibleArea64.contains(curr);
                bool draw = false;
                if (prevIn && currIn)
                {
                    draw = true;
                    if (withColors)
                    {
                        colorInterFrom = colorSubFrom;
                        colorInterTo = colorSubTo;
                    }
                }
                else
                {
                    if (Utilities::calculateIntersection(curr, prev, visibleArea, inter1))
                    {
                        draw = true;
                        if (prevIn)
                        {
                            drawTo = inter1;
                            if (withColors)
                            {
                                colorInterFrom = colorSubFrom;
                                const auto factor =
                                    static_cast<double>((curr - prev).norm()) /
                                    static_cast<double>((drawTo - prev).norm());
                                colorInterTo = middleColor(colorSubFrom, colorSubTo, static_cast<float>(factor));
                            }
                        }
                        else if (currIn)
                        {
                            drawFrom = inter1;
                            segmentStarted = false;
                            if (withColors)
                            {
                                const auto factor =
                                    static_cast<double>((curr - prev).norm()) /
                                    static_cast<double>((drawFrom - prev).norm());
                                colorInterFrom = middleColor(colorSubFrom, colorSubTo, static_cast<float>(factor));
                                colorInterTo = colorSubTo;
                            }
                        }
                        else if (Utilities::calculateIntersection(prev, curr, visibleArea, inter2))
                        {
                            drawFrom = inter1;
                            drawTo = inter2;
                            segmentStarted = false;
                            if (withColors)
                            {
                                auto factor =
                                    static_cast<double>((curr - prev).norm()) /
                                    static_cast<double>((drawFrom - prev).norm());
                                colorInterFrom = middleColor(colorSubFrom, colorSubTo, static_cast<float>(factor));
                                factor =
                                    static_cast<double>((curr - prev).norm()) /
                                    static_cast<double>((drawTo - prev).norm());
                                colorInterTo = middleColor(colorSubFrom, colorSubTo, static_cast<float>(factor));
                            }
                        }
                        else
                            draw = false;
                    }
                }
                if (draw)
                {
                    if (!segmentStarted)
                    {
                        if (!segment.empty())
                        {
                            segments.push_back(segment);
                            segment = std::vector<PointI>();
                            segmentColors.push_back(colors);
                            colors.clear();
                        }
                        segment.push_back(PointI(drawFrom));
                        if (withColors)
                            colors.push_back(colorInterFrom);
                        segmentStarted = currIn;
                    }
                    PointI drawTo31(drawTo);
                    if (segment.empty() || segment.back() != drawTo31)
                    {
                        segment.push_back(drawTo31);
                        if (withColors)
                            colors.push_back(colorInterTo);
                    }
                }
                else
                    segmentStarted = false;
                prevIn = currIn;
                prev = drawFrom = curr;
            }
            if (!segment.empty())
                segments.push_back(segment);
            if (!colors.empty())
                segmentColors.push_back(colors);
            segment.clear();
            colors.clear();
        }
    }
}

float OsmAnd::VectorLine_P::zoom() const
{
    return _mapZoomLevel + (_mapVisualZoom >= 1.0f ? _mapVisualZoom - 1.0f : (_mapVisualZoom - 1.0f) * 2.0f);
}

std::shared_ptr<OsmAnd::OnSurfaceVectorMapSymbol> OsmAnd::VectorLine_P::generatePrimitive(const std::shared_ptr<OnSurfaceVectorMapSymbol> vectorLine)
{
    int order = owner->baseOrder;
    float zoom = this->zoom();
    double scale = Utilities::getPowZoom(31 - zoom) * qSqrt(zoom) / (AtlasMapRenderer::TileSize3D * AtlasMapRenderer::TileSize3D); // TODO: this should come from renderer

    double visualShiftCoef = 1 / (1 + _mapVisualZoomShift);
    double radius = _lineWidth * scale * visualShiftCoef;
    double outlineRadius = _outlineWidth * scale * visualShiftCoef;
    bool approximate = _isApproximationEnabled;
    double simplificationRadius = _lineWidth * scale * visualShiftCoef;

    vectorLine->order = order++;
    vectorLine->primitiveType = VectorMapSymbol::PrimitiveType::Triangles;
    vectorLine->scaleType = VectorMapSymbol::ScaleType::In31;
    vectorLine->scale = 1.0;
    vectorLine->direction = 0.f;

    const auto verticesAndIndices = std::make_shared<VectorMapSymbol::VerticesAndIndices>();
    // Line has no reusable vertices - TODO clarify
    verticesAndIndices->indices = nullptr;
    verticesAndIndices->indicesCount = 0;

    clearArrowsOnPath();

    std::vector<VectorMapSymbol::Vertex> vertices;
    VectorMapSymbol::Vertex vertex;

    std::vector<std::vector<PointI>> segments;
    QList<QList<FColorARGB>> colors;
    calculateVisibleSegments(segments, colors);

    PointD startPos;
    bool startPosDefined = false;
    for (int segmentIndex = 0; segmentIndex < segments.size(); segmentIndex++)
    {
        auto& points = segments[segmentIndex];
        auto colorsForSegment = hasColorizationMapping() ? colors[segmentIndex] : QList<FColorARGB>();
        if (points.size() < 2)
            continue;

        if (!startPosDefined)
        {
            startPosDefined = true;
            const auto startPoint = points[0];
            int64_t intFull = INT32_MAX;
            intFull++;
            const auto intHalf = intFull >> 1;
            const auto intTwo = intFull << 1;
            const PointI64 origPos = PointI64(intHalf, intHalf) + startPoint;
            const PointI location31(
                origPos.x > INT32_MAX ? origPos.x - intTwo : origPos.x,
                origPos.y > INT32_MAX ? origPos.y - intTwo : origPos.y);
            vectorLine->position31 = location31;
            verticesAndIndices->position31 = new PointI(location31);
            startPos = PointD(startPoint);
        }
        int pointsCount = (int) points.size();

        std::vector<bool> include(pointsCount, !approximate);
        if (approximate)
        {
            include[0] = true;
            simplifyDouglasPeucker(points, 0, (uint) points.size() - 1, simplificationRadius / 3, include);
        }

        std::vector<OsmAnd::PointD> pointsToPlot(pointsCount);
        QSet<uint> colorChangeIndexes;
        uint prevPointIdx = 0;
        int includedPointsCount = 0;
        for (auto pointIdx = 0u; pointIdx < pointsCount; pointIdx++)
        {
            pointsToPlot[pointIdx] = PointD(points[pointIdx]) - startPos;
            if (hasColorizationMapping() && _colorizationSceme == COLORIZATION_SOLID)
            {
                FColorARGB prevColor = colorsForSegment[prevPointIdx];
                if (prevColor != colorsForSegment[pointIdx])
                {
                    colorChangeIndexes.insert(pointIdx);
                    prevColor = colorsForSegment[pointIdx];
                }
                prevPointIdx = pointIdx;
            }

            // If color is lost after approximation, restore it
            if (approximate && !include[pointIdx])
                include[pointIdx] = forceIncludePoint(colorsForSegment, pointIdx);

            if (include[pointIdx])
                includedPointsCount++;
        }

        if (_showArrows && owner->pathIcon)
        {
            const auto arrowsOrigin = segments.back().back();
            addArrowsOnSegmentPath(points, include, arrowsOrigin);
        }

        // generate base points for connecting lines with triangles
        std::vector<OsmAnd::PointD> b1(includedPointsCount), b2(includedPointsCount), e1(includedPointsCount), e2(includedPointsCount), original(includedPointsCount);
        double ntan = 0, nx1 = 0, ny1 = 0;
        prevPointIdx = 0;
        uint insertIdx = 0;

        QVector<uint> solidColorChangeIndexes;
        QList<OsmAnd::FColorARGB> filteredColorsMap;
        for (auto pointIdx = 0u; pointIdx < pointsCount; pointIdx++)
        {
            if (!include[pointIdx])
                continue;

            if (hasColorizationMapping())
            {
                const auto& color = colorsForSegment[pointIdx];
                filteredColorsMap.push_back(color);
                if (_colorizationSceme == COLORIZATION_SOLID && colorChangeIndexes.contains(pointIdx))
                    solidColorChangeIndexes.push_back(insertIdx);
            }

            PointD pnt = pointsToPlot[pointIdx];
            PointD prevPnt = pointsToPlot[prevPointIdx];
            original[insertIdx] = pnt;
            if (pointIdx > 0)
            {
                ntan = atan2(pnt.x - prevPnt.x, pnt.y - prevPnt.y);
                nx1 = radius * sin(M_PI_2 - ntan) ;
                ny1 = radius * cos(M_PI_2 - ntan) ;
                e1[insertIdx] = b1[insertIdx] = OsmAnd::PointD(pnt.x - nx1, pnt.y + ny1);
                e2[insertIdx] = b2[insertIdx] = OsmAnd::PointD(pnt.x + nx1, pnt.y - ny1);
                e1[insertIdx - 1] = OsmAnd::PointD(prevPnt.x - nx1, prevPnt.y + ny1);
                e2[insertIdx - 1] = OsmAnd::PointD(prevPnt.x + nx1, prevPnt.y - ny1);
            }
            else
            {
                b2[insertIdx] = b1[insertIdx] = pnt;
            }
            prevPointIdx = pointIdx;
            insertIdx++;
        }

        //OsmAnd::LogPrintf(OsmAnd::LogSeverityLevel::Info, "=== pointsCount=%d zoom=%d visualZoom=%f metersPerPixel=%f radius=%f simpleCount=%d cnt=%d", verticesAndIndices->verticesCount,
        //  _mapZoomLevel, _mapVisualZoom, _metersPerPixel, radius, includedPointsCount,pointsCount);
        //FColorARGB fillColor = FColorARGB(0.6, 1.0, 0.0, 0.0);
        FColorARGB fillColor = _fillColor;
        int patternLength = (int) _dashPattern.size();
        // generate triangles
        if (patternLength > 0)
        {
            // Dashed line does not support colorization yet, thus clear colors map
            filteredColorsMap.clear();
            
            std::vector<double> dashPattern(_dashPattern);
            double threshold = dashPattern[0] < 0 ? -dashPattern[0] : 0;
            if (threshold > 0)
            {
                dashPattern.erase(dashPattern.begin());
                patternLength--;
            }

            OsmAnd::PointD start = original[0];
            OsmAnd::PointD end = original[original.size() - 1];
            OsmAnd::PointD prevPnt = start;

            std::vector<OsmAnd::PointD> origTar;
            if (threshold == 0)
                origTar.push_back(start);

            double dashPhase = 0;
            int patternIndex = 0;
            bool firstDash = true;
            for (auto pointIdx = 1u; pointIdx < includedPointsCount; pointIdx++)
            {
                OsmAnd::PointD pnt = original[pointIdx];
                double segLength = sqrt(pow((prevPnt.x - pnt.x), 2) + pow((prevPnt.y - pnt.y), 2));
                // create a vector of direction for the segment
                OsmAnd::PointD v = pnt - prevPnt;
                // unit length
                OsmAnd::PointD u(v.x / segLength, v.y / segLength);

                double length = firstDash && threshold > 0 ? threshold * scale : dashPattern[patternIndex] * scale - 0;
                bool gap = firstDash && threshold > 0 ? true : patternIndex % 2 == 1;

                OsmAnd::PointD delta;
                double deltaLength;
                if (dashPhase == 0)
                    deltaLength = length;
                else
                    deltaLength = dashPhase;

                delta = OsmAnd::PointD(u.x * deltaLength, u.y * deltaLength);

                if (segLength <= deltaLength)
                {
                    if (!gap)
                        origTar.push_back(pnt);
                }
                else
                {
                    while (deltaLength < segLength)
                    {
                        origTar.push_back(prevPnt + delta);

                        if (!gap)
                        {
                            createVertexes(vertices, vertex, origTar, radius, fillColor, filteredColorsMap);
                            origTar.clear();
                            firstDash = false;
                        }

                        if (!firstDash)
                            patternIndex++;

                        patternIndex %= patternLength;
                        gap = patternIndex % 2 == 1;
                        length = dashPattern[patternIndex] * scale - (gap ? 0 : 0);
                        delta += OsmAnd::PointD(u.x * length, u.y * length);
                        deltaLength += length;
                    }

                    if (!origTar.empty() && !gap)
                        origTar.push_back(pnt);
                }

                // calculate dash phase
                dashPhase = length - (segLength - deltaLength);
                if (dashPhase > length)
                    dashPhase -= length;

                prevPnt = pnt;
            }
            // end point
            if (threshold == 0)
            {
                if (origTar.size() == 0)
                    origTar.push_back(end);
                
                origTar.push_back(end);
                createVertexes(vertices, vertex, origTar, radius, fillColor, filteredColorsMap);
            }
        }
        else if (_colorizationSceme == COLORIZATION_SOLID && !solidColorChangeIndexes.isEmpty())
        {
            uint prevIdx = 1;
            for (const uint idx : solidColorChangeIndexes)
            {
                const auto begin = original.begin() + prevIdx;
                const auto end = original.begin() + idx;
                std::vector<PointD> subvector(begin, end);
                const auto fillColor = filteredColorsMap[idx - 1];
                QList<FColorARGB> colors;
                for (int i = 0; i < idx - prevIdx; i++)
                    colors.push_back(fillColor);

                crushedpixel::Polyline2D::create<OsmAnd::VectorMapSymbol::Vertex, std::vector<OsmAnd::PointD>>(
                    vertex,
                    vertices,
                    subvector, radius * 2,
                    _fillColor, colors,
                    crushedpixel::Polyline2D::JointStyle::ROUND,
                    static_cast<crushedpixel::Polyline2D::EndCapStyle>(static_cast<int>(owner->endCapStyle)));
                prevIdx = idx - 1;
            }
            const auto begin = original.begin() + prevIdx;
            const auto end = original.end();
            std::vector<PointD> subvector(begin, end);
            const auto fillColor = filteredColorsMap.back();
            QList<FColorARGB> colors;
            for (int i = 0; i < original.size() - prevIdx; i++)
                colors.push_back(fillColor);
            crushedpixel::Polyline2D::create<OsmAnd::VectorMapSymbol::Vertex, std::vector<OsmAnd::PointD>>(
                vertex,
                vertices,
                subvector, radius * 2,
                _fillColor, colors,
                crushedpixel::Polyline2D::JointStyle::ROUND,
                static_cast<crushedpixel::Polyline2D::EndCapStyle>(static_cast<int>(owner->endCapStyle)));
        }
        else
        {
            // Drawing outline on relief is not supported 
            bool drawOutline = !qFuzzyIsNull(_outlineWidth) && !_hasElevationDataProvider;
            if (drawOutline)
                crushedpixel::Polyline2D::create<OsmAnd::VectorMapSymbol::Vertex, std::vector<OsmAnd::PointD>>(
                    vertex,
                    vertices,
                    original, outlineRadius * 2,
                    _outlineColor, QList<FColorARGB>(),
                    crushedpixel::Polyline2D::JointStyle::ROUND,
                    static_cast<crushedpixel::Polyline2D::EndCapStyle>(static_cast<int>(owner->endCapStyle)));

            crushedpixel::Polyline2D::create<OsmAnd::VectorMapSymbol::Vertex, std::vector<OsmAnd::PointD>>(
                vertex,
                vertices,
                original, radius * 2,
                _fillColor, filteredColorsMap,
                crushedpixel::Polyline2D::JointStyle::ROUND,
                static_cast<crushedpixel::Polyline2D::EndCapStyle>(static_cast<int>(owner->endCapStyle)));
        }
    }
    if (vertices.size() == 0)
    {
        vertex.positionXY[0] = 0;
        vertex.positionXY[1] = 0;
        vertices.push_back(vertex);
        verticesAndIndices->position31 = new PointI(0, 0);
    }

    // Tesselate the line for the surface
	auto partSizes =
		std::shared_ptr<std::vector<std::pair<TileId, int32_t>>>(new std::vector<std::pair<TileId, int32_t>>);
    const auto zoomLevel = _mapZoomLevel < MaxZoomLevel ? static_cast<ZoomLevel>(_mapZoomLevel + 1) : _mapZoomLevel;
	const auto cellsPerTileSize = (AtlasMapRenderer::HeixelsPerTileSide - 1) / (1 << zoomLevel - _mapZoomLevel);
    bool tesselated = _hasElevationDataProvider
        ? GeometryModifiers::overGrid(
            vertices,
            nullptr,
            vectorLine->primitiveType,
            partSizes,
            Utilities::getPowZoom(31 - zoomLevel),
            Utilities::convert31toDouble(*(verticesAndIndices->position31), zoomLevel),
            cellsPerTileSize,
            1.0f, 0.01f,
            false, false)
        : false;
	verticesAndIndices->partSizes = tesselated ? partSizes : nullptr;
    verticesAndIndices->zoomLevel = tesselated ? zoomLevel : InvalidZoomLevel;

    //verticesAndIndices->verticesCount = (includedPointsCount - 2) * 2 + 2 * 2;
    verticesAndIndices->verticesCount = (unsigned int) vertices.size();
    verticesAndIndices->vertices = new VectorMapSymbol::Vertex[vertices.size()];
    std::copy(vertices.begin(), vertices.end(), verticesAndIndices->vertices);

    vectorLine->isHidden = _isHidden;
    vectorLine->setVerticesAndIndices(verticesAndIndices);
    return vectorLine;
}

void OsmAnd::VectorLine_P::createVertexes(std::vector<VectorMapSymbol::Vertex> &vertices,
                  VectorMapSymbol::Vertex &vertex,
                  std::vector<OsmAnd::PointD> &original,
                  double radius,
                  FColorARGB &fillColor,
                  QList<FColorARGB>& colorMapping) const
{
    auto pointsCount = original.size();
    if (pointsCount == 0)
        return;

    crushedpixel::Polyline2D::create<OsmAnd::VectorMapSymbol::Vertex, std::vector<OsmAnd::PointD>>(
        vertex,
        vertices,
        original, radius * 2,
        fillColor, colorMapping,
        crushedpixel::Polyline2D::JointStyle::ROUND,
        static_cast<crushedpixel::Polyline2D::EndCapStyle>(static_cast<int>(owner->endCapStyle)));
}

void OsmAnd::VectorLine_P::clearArrowsOnPath()
{
    QWriteLocker scopedLocker(&_arrowsOnPathLock);

    _arrowsOnPath.clear();
}

const QList<OsmAnd::VectorLine::OnPathSymbolData> OsmAnd::VectorLine_P::getArrowsOnPath() const
{
    QReadLocker scopedLocker(&_arrowsOnPathLock);

    return detachedOf(_arrowsOnPath);
}

void OsmAnd::VectorLine_P::addArrowsOnSegmentPath(
    const std::vector<PointI>& segmentPoints,
    const std::vector<bool>& includedPoints,
    const PointI64& origin)
{
    SkPath path;
    const PointI64 start = segmentPoints.back();
    path.moveTo(start.x - origin.x, start.y - origin.y);
    for (int i = (int) segmentPoints.size() - 2; i >= 0; i--)
    {
        if (!includedPoints[i])
            continue;

        const auto& p = segmentPoints[i];
        path.lineTo(p.x - origin.x, p.y - origin.y);
    }

    SkPathMeasure pathMeasure(path, false);
    bool ok = false;
    const auto length = pathMeasure.getLength();

    float pathIconStep = getPointStepPx();

    float step = Utilities::metersToX31(pathIconStep * _metersPerPixel * owner->screenScale);
    auto iconOffset = 0.5f * step;
    const auto iconInstancesCount = static_cast<int>((length - iconOffset) / step) + 1;
    if (iconInstancesCount > 0)
    {
        int64_t intFull = INT32_MAX;
        intFull++;
        const auto intHalf = intFull >> 1;
        const auto intTwo = intFull << 1;
        const PointD location(origin);

        QWriteLocker scopedLocker(&_arrowsOnPathLock);

        for (auto iconInstanceIdx = 0; iconInstanceIdx < iconInstancesCount; iconInstanceIdx++, iconOffset += step)
        {
            SkPoint p;
            SkVector t;
            ok = pathMeasure.getPosTan(iconOffset, &p, &t);
            if (!ok)
                break;

            PointI64 origPos(
                static_cast<int64_t>((double)p.x() + location.x),
                static_cast<int64_t>((double)p.y() + location.y));
            origPos += PointI64(intHalf, intHalf);
            const PointI position(
                origPos.x + (origPos.x >= intFull ? -intTwo : (origPos.x < -intFull ? intFull : 0)),
                origPos.y + (origPos.y >= intFull ? -intTwo : (origPos.y < -intFull ? intFull : 0)));
            // Get mirrored direction
            float direction = Utilities::normalizedAngleDegrees(qRadiansToDegrees(atan2(-t.x(), t.y())) - 180);
            const VectorLine::OnPathSymbolData arrowSymbol(position, direction);
            _arrowsOnPath.push_back(arrowSymbol);
        }
    }
}

bool OsmAnd::VectorLine_P::useSpecialArrow() const
{
    return owner->specialPathIcon != nullptr && _lineWidth <= owner->specialPathIcon->width() + 3.0f;
}

double OsmAnd::VectorLine_P::getPointStepPx() const
{
    if (useSpecialArrow())
    {
        return _specialPathIconStep > 0
            ? _specialPathIconStep
            : owner->specialPathIcon->height() * SPECIAL_ARROW_DISTANCE_MULTIPLIER;
    }
    else
    {
        return _pathIconStep > 0 ? _pathIconStep : _scaledPathIcon->height();
    }
}

sk_sp<const SkImage> OsmAnd::VectorLine_P::getPointImage() const
{
    return useSpecialArrow() ? owner->specialPathIcon : _scaledPathIcon;
}

