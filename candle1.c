#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>

#define FILE_COLUMNS 6
#define UNIX_EPOCH_COLUMN 0
#define OPEN_PRICE_COLUMN 1
#define HIGH_PRICE_COLUMN 2
#define LOW_PRICE_COLUMN 3
#define CLOSE_PRICE_COLUMN 4
#define VOLUME_COLUMN 5

/* Structure to store OHLC data */
typedef struct {
  time_t unix_epoch;
  double open_price;
  double high_price;
  double low_price;
  double close_price;
  long volume;
} OHLCData;

/* Function to parse a line from the file and return an OHLCData structure */
OHLCData parse_line(char *line) {
  OHLCData data;
  char *token;

  /* Parse UNIX epoch */
  token = strtok(line, ",");
  data.unix_epoch = atoi(token);

  /* Parse open price */
  token = strtok(NULL, ",");
  data.open_price = atof(token);

  /* Parse high price */
  token = strtok(NULL, ",");
  data.high_price = atof(token);

  /* Parse low price */
  token = strtok(NULL, ",");
  data.low_price = atof(token);

  /* Parse close price */
  token = strtok(NULL, ",");
  data.close_price = atof(token);

  /* Parse volume */
  token = strtok(NULL, ",");
  data.volume = atol(token);

  return data;
}

/* Function to draw a financial candlestick */
void draw_candlestick(Display *display, Window window, GC gc, int x, int y, int width, int open, int high, int low, int close) {
  /* Calculate the body and wick lengths */
  int body_length = abs(open - close);
  int wick_length = high - low - body_length;

  /* Set the color based on the direction of the candlestick */
  if (open < close) {
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
  } else {
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
  }

  /* Draw the body */
  XFillRectangle(display, window, gc, x, y + low, width, body_length);

  /* Draw the wick */
  XDrawLine(display, window, gc, x + (width / 2), y + high, x + (width / 2), y + low + body_length);
}

int main(int argc, char *argv[]) {
  Display *display;
  Window window;
  GC gc;
  XEvent event;
  Atom delete_window;
  int screen;
  XmString str;
  Widget shell, drawing_area;
  XmDrawingAreaCallbackStruct call_data;
  char line[1024];
  int num_data_points = 0;
  OHLCData *data;
  int i;
  int x_offset, y_offset;
  int x, y;
  int width;
  double min_price, max_price;
  double price_range;
  double scale;
  int height;

  /* Check for the correct number of arguments */
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <file>\n", argv[0]);
    exit(1);
  }

  /* Open the file and count the number of data points */
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    fprintf(stderr, "Error: unable to open file %s\n", argv[1]);
    exit(1);
  }
  while (fgets(line, 1024, fp)) {
    num_data_points++;
  }
  rewind(fp);

  /* Allocate memory for the data points */
  data = malloc(num_data_points * sizeof(OHLCData));

  /* Read the data from the file and store it in the data array */
  i = 0;
  while (fgets(line, 1024, fp)) {
    data[i++] = parse_line(line);
  }
  fclose(fp);

  /* Initialize the X11 display */
  display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Error: unable to open display\n");
    exit(1);
  }

  /* Get the screen number */
  screen = DefaultScreen(display);

  /* Create the window */
  window = XCreateSimpleWindow(display, RootWindow(display, screen),
                               50, 50, 800, 600, 1,
                               BlackPixel(display, screen),
                               WhitePixel(display, screen));

  /* Create the graphics context */
  gc = XCreateGC(display, window, 0, NULL);

  /* Set the window title */
  str = XmStringCreateLocalized("Financial Candlestick");
  XtVaSetValues(shell, XmNtitle, str, NULL);
  XmStringFree(str);

  /* Set the delete window protocol */
  delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &delete_window, 1);

  /* Create the Motif application */
  shell = XtVaAppInitialize(&app, "FinancialCandlestick", NULL, 0, &argc, argv, NULL, NULL);

  /* Create the drawing area widget */
  drawing_area = XtVaCreateManagedWidget("drawingArea",xmDrawingAreaWidgetClass,
                                         shell,
                                         NULL);

  /* Set the expose callback for the drawing area */
  XtAddCallback(drawing_area, XmNexposeCallback, expose_callback, NULL);

  /* Map the window and wait for events */
  XMapWindow(display, window);
  XtAppMainLoop(app);

  /* Free the data array */
  free(data);

  /* Close the display */
  XCloseDisplay(display);

  return 0;
}

/* Callback function for the expose event */
void expose_callback(Widget widget, XtPointer client_data, XtPointer call_data) {
  XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct *) call_data;
  Display *display = XtDisplay(widget);
  Window window = XtWindow(widget);
  GC gc = XDefaultGC(display, DefaultScreen(display));
  int width = cbs->width;
  int height = cbs->height;
  int i;

  /* Calculate the offsets and scaling factor */
  x_offset = 50;
  y_offset = 50;
  width = (width - 100) / num_data_points;
  min_price = data[0].low_price;
  max_price = data[0].high_price;
  for (i = 1; i < num_data_points; i++) {
    if (data[i].low_price < min_price) {
      min_price = data[i].low_price;
    }
    if (data[i].high_price > max_price) {
      max_price = data[i].high_price;
    }
  }
  price_range = max_price - min_price;
  scale = (double) (height - 100) / price_range;

  /* Draw the candlesticks */
  x = x_offset;
  for (i = 0; i < num_data_points; i++) {
    y = y_offset + (int) ((max_price - data[i].high_price) * scale);
    draw_candlestick(display, window, gc, x, y, width,
                     (int) ((data[i].open_price - min_price) * scale),
                     (int) ((data[i].high_price - min_price) * scale),
                     (int) ((data[i].low_price - min_price) * scale),
                     (int) ((data[i].close_price - min_price) * scale));
    x += width;
  }
}
