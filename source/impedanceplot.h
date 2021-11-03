#ifndef IMPEDANCEPLOT_H
#define IMPEDANCEPLOT_H

#include <QWidget>
#include <cmath>


/* Impedance Plot is a class built to plot impedances as points, and lines to support them. The y scale can be either linear or logarithmic, and the x scale is always linear.
 * The y axis can represent either magnitudes of impedances (Ohms), or phases (degrees).
 * The x axis can represent either time (look at one channel's impedance over time) or channels (look at the most recent impedance for many channels at once) */


/* Constants used to determine the sizes of various elements in the plot */

//Heights and widths of the graph and main pixmap
const int mainHeight = 315; //Height of the main pixmap (in pixels)
const int mainWidth = 825; //Width of the main pixmap (in pixels)
const double graphVerticalRatio = 0.8; //Ratio of the height of the graph to the height of the main pixmap
const double graphHorizontalRatio = 0.85; //Ratio of the width of the graph to the width of the main pixmap
const int graphHeight = round(mainHeight * graphVerticalRatio); //Height of the graph (in pixels)
const int graphWidth = round(mainWidth * graphHorizontalRatio); //Width of the graph (in pixels)

//Sizes of the axes ticks
const double xTickRatio = 0.03; //Ratio of the vertical x-axis ticks to the height of the graph
const double yTickRatio = 0.005; //Ratio of the horizontal y-axis ticks to the width of the graph
const double yLargeTickRatio = 0.01; //Ratio of large order of magnitude horizontal y-axis ticks to the width of the graph

//Coordinates of the origin of the graph, in reference to the main pixmap
const int graphOriginX = round(0.5*(1 - graphHorizontalRatio)*mainWidth); //X position of the origin of the graph in reference to the main pixmap (in pixels)
const int graphOriginY = round(0.5*(1 - graphVerticalRatio)*mainHeight); //Y position of the origin of the graph in reference to the main pixmap (in pixels)

//Sizes of the various points or lines to plot
const int redLineWidth = 3; //Width of the red line (generally used to display pulses)
const int defaultLineWidth = 1; //Width of a default line
const int smallCircleRadius = 1; //Radius of the small circle drawn when a point is plotted (in pixels)
const int largeCircleRadius = 4; //Radius of the large circle drawn when a point is plotted (in pixels)
const int triangleHeight = 10; //Height of the triangle drawn when a point is plotted above or below the range of the graph


/* Y axis can have either a linear or logarithmic scale */
enum YScale {
    Linear,
    Logarithmic
};


/* Points can be in the range of the y-axis, can clip high, or clip low */
enum ClipState{
    ClipHigh,
    InRange,
    ClipLow
};


/* Lines to be plotted are specified by the lines themselves, and their colors */
struct LineInfo {
    QLine line;
    QColor color;
};


/* Points to be plotted are specified by the points themselves, whether they are clipped (high or low), and whether they are highlighted */
struct PointInfo {
    QPoint point;
    ClipState clipState;
    bool highlighted;
};

class QPixmap;
class ImpedancePlot : public QWidget
{
    Q_OBJECT
public:
    explicit ImpedancePlot(QWidget *parent); //Constructor
    void redrawPlot(); //Redraw the entire plot, including labels, points, lines, and tick marks
    void setDomain(bool autoscale, double max); //Set the domain of the graph. Minimum is assumed to be zero. If autoscale is true, determine the tick marks intelligently
    void setRange(bool linear, int min = 4, int max = 7); //Set the range of the graph. If linear, range is assumed to be +/- 180 degrees. If not, it is a semilog graph default range 10^4 to 10^7
    void plotPoint(double xCoordUnits, double yCoordUnits, ClipState state, bool highlighted = false); //Plot the point with the given coordinates, in the units of the graph. If the point should be large, set 'highlighted' true
    void plotLine(double x1units, double y1units, double x2units, double y2units, QColor color); //Plot the line with the given endpoints, in the units of the graph, as well as the given color
    void plotGrid(bool showGrid);

    QString title; //Title written above the graph
    QString xLabel; //Label (for units) written below the x-axis
    QString yLabel; //Label (for units) written to the left of the y-axis

private:
    void paintEvent(QPaintEvent *event); //Reimplemented function that draws the pixmap
    void mousePressEvent(QMouseEvent *event) override; //Reimplemented function that gathers the position of a left mouse click, and emits the 'clickedXUnits' signal to return the x coordinate of the click in units of the graph
    QSize minimumSizeHint() const; //Reimplemented function that sets the minimum size hint to the size of the main pixmap
    double getClickXPixel(QPoint point); //Return the x pixel of a click with respect to the origin of the graph. If it is outside the bounds of the graph, return -1
    int convertToXPixels(double xunits); //Convert an x coordinate from the units of the graph to pixels
    int convertToYPixels(double yunits); //Convert a y coordinate from the units of the graph to pixels
    double convertToXUnits(double xcoord); //Convert an x coordinate from the pixels of the graph to units
    void drawRect(); //Draw a white rectangle onto the center of mainPixmap
    void drawGrid();
    void drawLines(); //Draw the lines in 'linesToPlot' onto mainPixmap, and then clear 'linesToPlot'
    void drawPoints(); //Draw the points in 'pointsToPlot' onto mainPixmap, and then clear 'pointsToPlot'
    void drawText(); //Draw title, x-axis label, and y-axis label onto mainPixmap
    void drawXAxis(); //Draw ticks and label them on the x axis, either calculating them if autoscaled, or drawing a 0-127 scale if not  
    void scaleAlgorithm(); //Algorithm used to determine the number and value of ticks on the x-axis
    void drawYAxis(); //Draw ticks and label them on the y axis, either on a Linear or Logarithmic scale depending on yScaleState

    QPixmap *mainPixmap; //Main pixmap that everything in this plot is drawn to
    QColor backgroundColor; //Stores the background color of the previous widget
    double xmax; //Maximum value of the domain
    double xscalemax; //Maximum value of the SCALE of the domain (can be above xmax, which is the maximum value to actually be plotted)
    int yminexp; //Minimum value of the exponent dictating the range, such that the smallest y value that can be graphed is 10^yminexp
    int ymaxexp; //Maximum value of the exponent dictating the range, such that the largest y value that can be graphed is 10^ymaxexp
    bool xautoscale; //Store whether or not this plot's x-axis should autoscale based on the domain set in 'SetDomain()'
    int numTicks;
    double valTicks;
    bool grid;
    YScale yScaleState; //Store whether the y axis is on a Logarithmic or Linear scale
    QVector<PointInfo> pointsToPlot; //Vector that store all points set in public 'plotPoint()' that are then drawn in private function 'drawPoints()'
    QVector<LineInfo> linesToPlot; //Vector that stores all lines set in public 'plotLine()' that are then drawn in private function 'drawLines()'

signals:
    void clickedXUnits(double); //Signal emitted that returns the x coordinate of a click, in the units of the graph. If the click was outside the graph, return -1
};

#endif // IMPEDANCEPLOT_H
