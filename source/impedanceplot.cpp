#include "impedanceplot.h"
#include "globalconstants.h"

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QtWidgets>
#endif

ImpedancePlot::ImpedancePlot(QWidget *parent) :
    QWidget(parent)
{
    //Store the background color from the parent widget (to be used later to fill the main pixmap)
    backgroundColor = parent->palette().background().color();

    //Initialize titlxe, xLabel, and yLabel QStrings
    title = "Default title";
    xLabel = "Default x-label";
    yLabel = "Default y-label";

    //Initialize members determining the scale of the x and y axes
    xmax = 1;
    yminexp = 0;
    ymaxexp = 0;
    xautoscale = false;
    yScaleState = Logarithmic;
    grid = false;

    //Allocate memory for mainPixmap
    mainPixmap = new QPixmap(mainWidth, mainHeight);

    //Draw the plot, with all the default values
    redrawPlot();
}


/* Public - Redraw the entire plot, including labels, points, lines, and tick marks */
void ImpedancePlot::redrawPlot()
{
    //Clear main pixmap
    mainPixmap->fill(backgroundColor);

    //Draw GRAPH rect at the center of the main pixmap
    drawRect();

    drawGrid();

    //Draw each line in linesToPlot, and clear the linesToPlot QVector
    drawLines();

    //Draw xAxis
    drawXAxis();

    //Draw yAxis
    drawYAxis();

    //Draw each point in pointsToPlot, and clear the pointsToPlot QVector
    drawPoints();

    //Draw TITLE, XLABEL, and YLABEL string
    drawText();

    update();
}


/* Public - Set the domain of the graph. Minimum is assumed to be zero. If autoscale is true, determine the tick marks intelligently */
void ImpedancePlot::setDomain(bool autoscale, double max)
{
    xautoscale = autoscale;
    xmax = max;

    if (xautoscale)
        scaleAlgorithm();
    else
        xscalemax = xmax;
}


/* Public - Set the range of the graph. If linear, range is assumed to be +/- 180 degrees. If not, it is a semilog graph default range 10^4 to 10^7 */
void ImpedancePlot::setRange(bool linear, int min, int max)
{
    if (linear) {
        yScaleState = Linear;
    }
    else {
        yScaleState = Logarithmic;
        yminexp = min;
        ymaxexp = max;
    }
}


/* Public - Plot the point with the given coordinates, in the units of the graph. If the point should be highlighted, set 'highlighted' true */
void ImpedancePlot::plotPoint(double xCoordUnits, double yCoordUnits, ClipState state, bool highlighted)
{   
    //Convert x and y coordinates to pixels
    double xPixels = convertToXPixels(xCoordUnits);
    double yPixels = convertToYPixels(yCoordUnits);

    PointInfo thisPoint;
    //Add offset to x and y to change reference to mainPixmap
    thisPoint.point = QPoint(xPixels + graphOriginX, yPixels + graphOriginY);
    //Set the 'clipState' member of this point
    thisPoint.clipState = state;
    //Set the 'highlighted' member of this point
    thisPoint.highlighted = highlighted;

    //Add this point to pointsToPlot
    pointsToPlot.append(thisPoint);
}


/* Public - Plot the line with the given endpoints, in the units of the graph, as well as the given color */
void ImpedancePlot::plotLine(double x1units, double y1units, double x2units, double y2units, QColor color)
{
    //Convert x and y coordinates to pixels
    double x1pixels = convertToXPixels(x1units);
    double y1pixels = convertToYPixels(y1units);
    double x2pixels = convertToXPixels(x2units);
    double y2pixels = convertToYPixels(y2units);

    if (x2units == xmax + 1)
        x2pixels = graphWidth;

    LineInfo thisLine;
    //Add offset to x and y to change reference to mainPixmap
    thisLine.line.setLine(graphOriginX + x1pixels, graphOriginY + y1pixels, graphOriginX + x2pixels, graphOriginY + y2pixels);
    //Set the 'color' member of this line
    thisLine.color = color;

    //Add this line to linesToPlot
    linesToPlot.append(thisLine);
}


/*  */
void ImpedancePlot::plotGrid(bool showGrid)
{
    grid = showGrid;
}


/* Reimplemented - Draw the pixmap */
void ImpedancePlot::paintEvent(QPaintEvent *event)
{
    QStylePainter stylePainter(this);
    stylePainter.drawPixmap(0, 0, *mainPixmap);
}


/* Reimplemented - Gather the position of a left mouse click, and emit the 'clickedXUnits' signal to return the x coordinate of the click in units of the graph */
void ImpedancePlot::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        double xcoord = getClickXPixel(event->pos());
        if (xcoord < 0)
            emit clickedXUnits(-1);
        else
            emit clickedXUnits(convertToXUnits(xcoord));
    }
}


/* Reimplemented - Set the minimum size hint to the size of the main pixmap */
QSize ImpedancePlot::minimumSizeHint() const
{
    return QSize(mainWidth, mainHeight);
}


/* Private - Return the x pixel of a click with respect to the graph origin. If it is outside the bounds of the graph, return -1 */
double ImpedancePlot::getClickXPixel(QPoint point)
{
    //Change reference of point from mainPixmap to graph origin
    QPoint graphPoint(point.x() - graphOriginX, point.y() - graphOriginY);

    //If point is within the bounds of the graph, return the x coordinate
    if ((graphPoint.x() >= 0 && graphPoint.x() <= graphWidth) &&
            (graphPoint.y() >= 0 && graphPoint.y() <= graphHeight - 1)) {
        return graphPoint.x();
    }

    //If the point is outside the bounds of the graph, return -1
    else
        return -1;
}


/* Private - Convert an x coordinate from the units of the graph to pixels */
int ImpedancePlot::convertToXPixels(double xunits)
{
    //Convert x coordinate
    double ratio = xunits / xscalemax;
    double xpixels = ratio * graphWidth;

    //If the x  coordinate is on the edge of the graph, move it one pixel closer to center
    if (xpixels <= 0)
        xpixels = 1;
    if (xpixels >= graphWidth)
        xpixels = graphWidth - 1;

    return round(xpixels);
}


/* Private - Convert a y coordinate from the units of the graph to pixels */
int ImpedancePlot::convertToYPixels(double yunits)
{
    //Convert y coordinate
    double ratio;
    if (yScaleState == Linear) {
        double yDegrees = yunits * RADIANS_TO_DEGREES;
        ratio = (180 - yDegrees)/360;
    }
    else {
        double yLog = log10(yunits);
        ratio = 1 - ((yLog - yminexp)/(ymaxexp - yminexp));
    }
    int ypixels = round(ratio * graphHeight);

    //If the y coordinate is on the edge of the graph, move it one pixel closer to center
    if (ypixels <= 1)
        ypixels = 2;
    if (ypixels >= graphHeight)
        ypixels = graphHeight - 1;

    return ypixels;
}


/* Private - Convert an x coordinate from the pixels of the graph to units */
double ImpedancePlot::convertToXUnits(double xcoord)
{
    double ratio = xcoord/graphWidth;
    return ratio * xmax;
}


/* Private - Draw a white rectangle onto the center of mainPixmap */
void ImpedancePlot::drawRect()
{
    QPainter painter(mainPixmap);
    QRect graphRect(QPoint(graphOriginX, graphOriginY), QPoint(graphOriginX + graphWidth - 1, graphOriginY + graphHeight - 1));
    painter.setBrush(Qt::white);
    painter.drawRect(graphRect);
    painter.end();
}


/*  */
void ImpedancePlot::drawGrid()
{
    QPainter painter(mainPixmap);

    if (grid) {
        if (yScaleState == Linear) {
            for (double i = 1; i <= 7; i++) {
                //Draw light gray grid line
                QPen pen(Qt::lightGray);
                painter.setPen(pen);
                painter.drawLine(graphOriginX, graphOriginY + graphHeight*(i*45/360), graphOriginX + graphWidth, graphOriginY + graphHeight*(i*45/360));
            }
        }
        else {
            int numSpaces = ymaxexp - yminexp;
            double orderHeight = graphHeight/numSpaces;
            for (double i = 1; i < numSpaces; i++) {
                //Draw dark gray grid line
                QPen pen(Qt::darkGray);
                painter.setPen(pen);
                painter.drawLine(graphOriginX, graphOriginY + graphHeight*i/numSpaces, graphOriginX + graphWidth, graphOriginY + graphHeight*i/numSpaces);
            }
            for (double i = 1; i <= numSpaces; i++) {
                double bottomOfOrder = graphHeight * i/numSpaces;
                for (int j = 2; j < 10; j++) {
                    //Draw light gray grid line
                    QPen pen(Qt::lightGray);
                    painter.setPen(pen);
                    painter.drawLine(graphOriginX, graphOriginY + bottomOfOrder - orderHeight*log10(j), graphOriginX + graphWidth, graphOriginY + bottomOfOrder - orderHeight*log10(j));
                }
            }
        }
    }
}


/* Private - Draw the lines in 'linesToPlot' onto mainPixmap, and then clear 'linesToPlot' */
void ImpedancePlot::drawLines()
{
    QPainter painter(mainPixmap);

    //Draw each line in linesToPlot
    for (int i = 0; i < linesToPlot.size(); i++) {
        //Anti-alias blue lines (typically used for non-horizontal lines)
        if (linesToPlot.at(i).color == Qt::blue)
            painter.setRenderHint(QPainter::Antialiasing);
        //Don't anti-alias non-blue lines (typically used for horizontal lines)
        else
            painter.setRenderHint(QPainter::Antialiasing, false);
        //Set red line to be drawn with width 'redLineWidth'
        if (linesToPlot.at(i).color == Qt::red) {
            QPen pen(Qt::red);
            pen.setWidth(redLineWidth);
            painter.setPen(pen);
        }
        //Set non-red line to be drawn with width 'defaultLineWidth'
        else {
            QPen pen(linesToPlot.at(i).color);
            pen.setWidth(defaultLineWidth);
            painter.setPen(pen);
        }
        //Draw line with whatever pen was previously set
        painter.drawLine(linesToPlot.at(i).line);
    }

    //Clear linesToPlot (so that for each redraw, linesToPlot is a blank slate)
    linesToPlot.clear();
    painter.end();
}


/* Private - Draw the points in 'pointsToPlot' onto mainPixmap, and then clear 'pointsToPlot' */
void ImpedancePlot::drawPoints()
{
    QPainter painter(mainPixmap);

    //Anti-alias all points
    painter.setRenderHint(QPainter::Antialiasing);

    //Draw each point in pointsToPlot
    for (int i = 0; i < pointsToPlot.size(); i++) {
        //If point is above the max range of the y-axis (clips high), display it as an empty red triangle pointing up (filled in when highlighted)
        if (pointsToPlot.at(i).clipState == ClipHigh) {
            bool filled = pointsToPlot.at(i).highlighted;
            painter.setPen(Qt::red);
            painter.setBrush(Qt::red);
            QVector<QPoint> trianglePoints;
            //Add bottom left corner
            trianglePoints.append(QPoint(pointsToPlot.at(i).point.x() - 0.5*triangleHeight, graphOriginY + 0.5*triangleHeight));
            //Add bottom right corner
            trianglePoints.append(QPoint(pointsToPlot.at(i).point.x() + 0.5*triangleHeight, graphOriginY + 0.5*triangleHeight));
            //Add top corner
            trianglePoints.append(QPoint(pointsToPlot.at(i).point.x(), graphOriginY - 0.5*triangleHeight));

            //If triangle should be filled, draw the triangle as a filled polygon
            if (filled) {
                QPolygon upTriangle(trianglePoints);
                painter.drawPolygon(upTriangle);
            }

            //If triangle should not be filled, draw the triangle as three lines
            else {
                //Make triangle smaller if not highlighted
                QVector<QPoint> smallerTrianglePoints;
                //Add bottom left corner
                smallerTrianglePoints.append(QPoint(trianglePoints.at(0).x() + 1, trianglePoints.at(0).y() - 1));
                //Add bottom right corner
                smallerTrianglePoints.append(QPoint(trianglePoints.at(1).x() - 1, trianglePoints.at(1).y() - 1));
                //Add top corner
                smallerTrianglePoints.append(QPoint(trianglePoints.at(2).x(), trianglePoints.at(2).y() + 1));

                painter.drawLine(smallerTrianglePoints.at(0), smallerTrianglePoints.at(1));
                painter.drawLine(smallerTrianglePoints.at(0), smallerTrianglePoints.at(2));
                painter.drawLine(smallerTrianglePoints.at(1), smallerTrianglePoints.at(2));
            }
        }

        //If point is below the min range of the y-axis, display it as an empty red triangle pointing down (filled in when highlighted)
        else if (pointsToPlot.at(i).clipState == ClipLow) {
            bool filled = pointsToPlot.at(i).highlighted;
            painter.setPen(Qt::red);
            painter.setBrush(Qt::red);
            QVector<QPoint> trianglePoints;
            //Add top left corner
            trianglePoints.append(QPoint(pointsToPlot.at(i).point.x() - 0.5*triangleHeight, graphHeight + graphOriginY - 0.5*triangleHeight));
            //Add top right corner
            trianglePoints.append(QPoint(pointsToPlot.at(i).point.x() + 0.5*triangleHeight, graphHeight + graphOriginY - 0.5*triangleHeight));
            //Add bottom corner
            trianglePoints.append(QPoint(pointsToPlot.at(i).point.x(), graphHeight + graphOriginY + 0.5*triangleHeight));

            //If triangle should be filled, draw the triangle as a filled polygon
            if (filled) {
                QPolygon downTriangle(trianglePoints);
                painter.drawPolygon(downTriangle);
            }

            //If triangle should not be filled, draw the triangle as three lines
            else {
                //Make triangle smaller if not highlighted
                QVector<QPoint> smallerTrianglePoints;
                //Add bottom left corner
                smallerTrianglePoints.append(QPoint(trianglePoints.at(0).x() + 1, trianglePoints.at(0).y() + 1));
                //Add bottom right corner
                smallerTrianglePoints.append(QPoint(trianglePoints.at(1).x() - 1, trianglePoints.at(1).y() + 1));
                //Add top corner
                smallerTrianglePoints.append(QPoint(trianglePoints.at(2).x(), trianglePoints.at(2).y() - 1));

                painter.drawLine(smallerTrianglePoints.at(0), smallerTrianglePoints.at(1));
                painter.drawLine(smallerTrianglePoints.at(0), smallerTrianglePoints.at(2));
                painter.drawLine(smallerTrianglePoints.at(1), smallerTrianglePoints.at(2));
            }
        }

        //If point is within the bounds of the graph, draw it as a blue circle of either small or large size, depending on if it is highlighted
        else {
            painter.setPen(Qt::blue);
            painter.setBrush(Qt::blue);
            int size = pointsToPlot.at(i).highlighted ? largeCircleRadius : smallCircleRadius;
            painter.drawEllipse(pointsToPlot.at(i).point, size, size);
        }
    }

    //Clear pointsToPlot (so that for each redraw, pointsToPlot is a blank state)
    pointsToPlot.clear();
    painter.end();
}


/* Private - Draw title, x-axis label, and y-axis label onto mainPixmap */
void ImpedancePlot::drawText()
{
    //Set up pen to draw text
    QPainter painter(mainPixmap);
    painter.setPen(Qt::black);

    //Draw TITLE string vertically centered above the graph
    QFont defaultFont = painter.font();
    QFont titleFont = painter.font();
    //titleFont.setPointSize(defaultFont.pointSize() * 2);
    titleFont.setPointSize(defaultFont.pointSize() * 1.5);
    painter.setFont(titleFont);
    QFontMetrics titleMetrics = painter.fontMetrics();
    painter.drawText(QPoint((mainWidth - titleMetrics.width(title))/2, titleMetrics.height()), title);
    painter.setFont(defaultFont);

    //Draw XLABEL string vertically centered below the graph
    QFont labelFont = painter.font();
    //labelFont.setPointSize(defaultFont.pointSize() * 1.5);
    labelFont.setPointSize(defaultFont.pointSize() * 1.2);
    painter.setFont(labelFont);
    QFontMetrics largeMetrics = painter.fontMetrics();
    int xLabelRowPosition = graphOriginY + (graphVerticalRatio * mainHeight) + (1.6 * largeMetrics.height());
    painter.drawText(QPoint((mainWidth - largeMetrics.width(xLabel))/2, xLabelRowPosition), xLabel);

    //Draw YLABEL string, rotated, horizontally centered to the left of the graph
    int yUnitsLabelPosition = mainHeight/2 + largeMetrics.width(yLabel)/2;
    painter.rotate(270);
    painter.drawText(QPoint(-1 * yUnitsLabelPosition, 1.5 * largeMetrics.height()), yLabel);

    painter.end();
}


/* Private - Draw ticks and label them on the x axis, either calculating them if autoscaled, or drawing a 0-127 scale if not */
void ImpedancePlot::drawXAxis()
{
    QPainter painter(mainPixmap);

    //Determine xUnitsRowPosition
    QFontMetrics metrics = this->fontMetrics();
    int xUnitsRowPosition = graphOriginY + (graphVerticalRatio * mainHeight) + (1 * metrics.height());

    //Autoscale - generate 5-10 ticks of suitable values to plot the maximum domain value set in 'xmax' (suitable for time as x-axis)
    if (xautoscale) {

        scaleAlgorithm();

        //Draw ticks between 0 and max
        for (double i = 1; i < numTicks; i++) {
            //Top ticks
            painter.drawLine(graphOriginX + (i/numTicks)*graphWidth, graphOriginY, graphOriginX + (i/numTicks)*graphWidth, graphOriginY + xTickRatio*graphHeight);
            //Bottom ticks
            painter.drawLine(graphOriginX + (i/numTicks)*graphWidth, graphOriginY + graphHeight, graphOriginX + (i/numTicks)*graphWidth, graphOriginY + (1 - xTickRatio)*graphHeight);
        }

        //Label all ticks
        for (double i = 0; i <= numTicks; i++) {
            painter.drawText(QPoint(graphOriginX + ((i/numTicks) * graphWidth) - (0.5 * metrics.width(QString::number(i * valTicks))), xUnitsRowPosition), QString::number(i * valTicks));
        }
    }

    //No autoscale - just draw hand-made x axis from 0 to 127 (suitable for channels as x-axis, i.e. currentZ graph)
    else {
        //Draw the 6 tick marks that mark channels 20, 40, 60, 80, 100, and 120
        for (double i = 1; i < 7; i++) {
            //Top ticks
            painter.drawLine(graphOriginX + ((i*20)/127)*graphWidth, graphOriginY, graphOriginX + ((i*20)/127)*graphWidth, graphOriginY + xTickRatio*graphHeight);
            //Bottom ticks
            painter.drawLine(graphOriginX + ((i*20)/127)*graphWidth, graphOriginY + graphHeight, graphOriginX + ((i*20)/127)*graphWidth, graphOriginY + (1 - xTickRatio)*graphHeight);
        }

        //Label ticks - all multiples of 20 up to 120
        for (double i = 0; i < 7; i++) {
            painter.drawText(QPoint(graphOriginX + (((i*20)/127)*graphWidth) - (0.5 * metrics.width(QString::number(i * 20))), xUnitsRowPosition), QString::number(i * 20));
        }
    }
    painter.end();
}


/* Algorithm used to determine the number and value of ticks on the x-axis */
void ImpedancePlot::scaleAlgorithm()
{
    /* Use marker algorithm to determine numTicks and valTicks */
    double scaledNum = xmax;
    valTicks = 1;

    //If scaledNum is high, bring it to the range [5, 50)
    while (scaledNum >= 50) {
        scaledNum = scaledNum / 10;
        valTicks = valTicks * 10;
    }
    //If scaledNum is low, bring it to the range [5, 50)
    while (scaledNum < 5) {
        scaledNum = scaledNum * 10;
        valTicks = valTicks / 10;
    }

    //If scaledNum is in the range [10, 20], divide by two to bring it to the range [5, 10]
    if (scaledNum >= 10 && scaledNum <= 20) {
        scaledNum = scaledNum / 2;
        valTicks = valTicks * 2;
    }
    //If scaledNum is in the range (20, 50), divide by five to bring it to the range (4, 10)
    else if (scaledNum > 10) {
        scaledNum = scaledNum / 5;
        valTicks = valTicks * 5;
    }

    //Round numTicks up to the next integer
    numTicks = ceil(scaledNum);

    xscalemax = numTicks * valTicks;
}


/* Private - Draw ticks and label them on the y axis, either on a Linear or Logarithmic scale depending on yScaleState */
void ImpedancePlot::drawYAxis()
{
    QPainter painter(mainPixmap);
    QFontMetrics metrics = this->fontMetrics();
    double midpoint;

    //Linear (phase) scale from 180 degrees to -180 degrees
    if (yScaleState == Linear) {
        //Draw 7 ticks at 45 degree increments from 135 to -135
        for (double i = 1; i <= 7; i++) {
            //Left ticks
            painter.drawLine(graphOriginX, graphOriginY + graphHeight*(i*45/360), graphOriginX + yTickRatio*graphWidth, graphOriginY + graphHeight*(i*45/360));
            //Right ticks
            painter.drawLine(graphOriginX + graphWidth, graphOriginY + graphHeight*(i*45/360), graphOriginX + (1 - yTickRatio)*graphWidth, graphOriginY + graphHeight*(i*45/360));
        }

        //Label ticks - all multiples of 45 degrees from 180 to -180
        for (double i = 0; i <= 8; i++) {
            midpoint = 0.5 * graphOriginX;
            painter.drawText(QPoint(midpoint + 0.8 * (midpoint - metrics.width(QString::number(180 - i*45))), graphOriginY + 0.25*metrics.height() + graphHeight*(i*45/360)), QString::number(180 - i*45));
        }
    }

    else {
        //Get number of orders of magnitude
        int numSpaces = ymaxexp - yminexp;

        //Draw large tick marks separating orders of magnitude
        for (double i = 1; i < numSpaces; i++) {
            //Left ticks
            painter.drawLine(graphOriginX, graphOriginY + graphHeight*i/numSpaces, graphOriginX + yLargeTickRatio*graphWidth, graphOriginY + graphHeight*i/numSpaces);
            //Right ticks
            painter.drawLine(graphOriginX + graphWidth, graphOriginY + graphHeight*i/numSpaces, graphOriginX + (1 - yLargeTickRatio)*graphWidth, graphOriginY + graphHeight*i/numSpaces);
        }
        //Draw small tick marks within each order of magnitude
        for (double i = 1; i <= numSpaces; i++) {
            double bottomOfOrder = graphHeight * i/numSpaces;
            double orderHeight = graphHeight/numSpaces;
            for (int j = 2; j < 10; j++) {
                //Left ticks
                painter.drawLine(graphOriginX, graphOriginY + bottomOfOrder - orderHeight*log10(j), graphOriginX + yTickRatio*graphWidth, graphOriginY + bottomOfOrder - orderHeight*log10(j));
                //Right ticks
                painter.drawLine(graphOriginX + graphWidth, graphOriginY + bottomOfOrder - orderHeight*log10(j), graphOriginX + (1 - yTickRatio)*graphWidth, graphOriginY + bottomOfOrder - orderHeight*log10(j));
            }
        }

        QString label;

        //Label ticks - each order of magnitude
        for (double i = 0; i <= numSpaces; i++) {
            midpoint = 0.5 * graphOriginX;
            switch ((int)i + yminexp) {
                case 0: label = "1"; break;
                case 1: label = "10"; break;
                case 2: label = "100"; break;
                case 3: label = "1k"; break;
                case 4: label = "10k"; break;
                case 5: label = "100k"; break;
                case 6: label = "1M"; break;
                case 7: label = "10M"; break;
                case 8: label = "100M"; break;
                case 9: label = "1G"; break;
                case 10: label = "10G"; break;
            }
            painter.drawText(QPoint(midpoint + 0.8*(midpoint - metrics.width(label)), graphOriginY + 0.25*metrics.height() + graphHeight*(numSpaces - i)/numSpaces), label);
        }
    }
    painter.end();
}
