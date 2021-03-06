#include "pebble.h"

#include "clock_digit.h"
#include "messagebox.h"

// main menu stuff
#define NUM_MENU_SECTIONS 3
#define NUM_1ST_MENU_ITEMS 3
#define NUM_2ND_MENU_ITEMS 1
#define NUM_3RD_MENU_ITEMS 1

// summary menu stuff
#define NUM_SUMMARY_MENU_ITEMS 6

// settings keys
#define PERSIST_KEY_DESIRED_LANE_COUNT 0
#define PERSIST_KEY_TIME_PER_LANE      1
#define PERSIST_KEY_LENGTH_OF_LANE     2
#define PERSIST_KEY_LAST_WORKOUT_LENGTH_OF_LANE   3
#define PERSIST_KEY_LAST_WORKOUT_LANE_COUNT       4
#define PERSIST_KEY_LAST_WORKOUT_START_OF_WORKOUT 5
#define PERSIST_KEY_LAST_WORKOUT_CUMULATIVE_PAUSE 6
#define PERSIST_KEY_LAST_WORKOUT_END_OF_WORKOUT   7

// settings variables
static int desiredLaneCount = 40;
static int timePerLane      = 36;
static int lengthOfLane     = 25;
int * currentValueToChange = NULL;

// layers
static Window * mainMenuWindow;
static MenuLayer * mainMenuLayer;
static ActionBarLayer *actionBarLayer;
static Window * summaryMenuWindow;
static MenuLayer * summaryMenuLayer;

// icons
static GBitmap * iconUp;
static GBitmap * iconOK;
static GBitmap * iconDown;
static GBitmap * iconPlay;
static GBitmap * iconPause;

// digits window
static Window * digitWindow;
static ClockDigit clockDigits[4];
static ActionBarLayer *digitActionBarLayer;

int laneCount = 0;
time_t startTimeOfWorkout  = 0;
int timeOfPreviousLane = 0;
time_t startTimeOfCurrentLane  = 0;
int cumulatedPauseTimeOfWorkout = 0;
int cumulatedPauseTimeOfCurrentLane = 0;
time_t startTimeOfCurrentPause = 0;
time_t virtualEndTimeOfCurrentLane = 0;

// values from last workout
int    lastWorkoutLengthOfLane = 0;
int    lastWorkoutLaneCount = 0;
time_t lastWorkoutStartTimeOfWorkout  = 0;
int    lastWorkoutCumulatedPauseTimeOfWorkout = 0;
time_t lastWorkoutEndTimeOfWorkout  = 0;

// forward declarations
static void startNextLane();
static void finishLane();
static void continueCurrentSwim();
static void setClickContextProviderForMainMenu(MenuLayer * menuLayer, Window * window);

//
// DigitWindow

static bool isPaused()
{
    return startTimeOfCurrentPause > 0;
}

static void updateDigitActionBarLayerIcons()
{
    if (isPaused()) {
        action_bar_layer_set_icon_animated(digitActionBarLayer, BUTTON_ID_SELECT, iconPlay, true);
    } else {
        action_bar_layer_set_icon_animated(digitActionBarLayer, BUTTON_ID_SELECT, iconPause, true);
    }
    action_bar_layer_set_icon_animated(digitActionBarLayer, BUTTON_ID_DOWN, iconOK, true);
}

static void quitCurrentSwim()
{
    finishLane();

    // remember last workout
    const time_t now = time(NULL);
    lastWorkoutLengthOfLane                = lengthOfLane;
    lastWorkoutLaneCount                   = laneCount;
    lastWorkoutStartTimeOfWorkout          = startTimeOfWorkout;
    lastWorkoutCumulatedPauseTimeOfWorkout = cumulatedPauseTimeOfWorkout;
    lastWorkoutEndTimeOfWorkout            = now;

    // calculate average swim time and set timePerLane
    const int swimTime = lastWorkoutEndTimeOfWorkout - lastWorkoutStartTimeOfWorkout - lastWorkoutCumulatedPauseTimeOfWorkout;
    const int avgTimePerLane = swimTime / lastWorkoutLaneCount;
    timePerLane = avgTimePerLane;

    window_stack_pop(false);
    window_stack_push(summaryMenuWindow, true);
}

static void setPaused(bool paused)
{
    if (paused == isPaused()) return;

    const time_t now = time(NULL);
    if (paused) {
        startTimeOfCurrentPause = now;
    } else {
        cumulatedPauseTimeOfWorkout     += now - startTimeOfCurrentPause;
        cumulatedPauseTimeOfCurrentLane += now - startTimeOfCurrentPause;
        startTimeOfCurrentPause = 0;

        virtualEndTimeOfCurrentLane = startTimeOfCurrentLane + timePerLane + cumulatedPauseTimeOfCurrentLane;
    }

    updateDigitActionBarLayerIcons();
}

static void onDigitActionBarLayerBackClicked(ClickRecognizerRef recognizer, void * context)
{
    setPaused(true);
    showMessageBox("Really finish current swim?", quitCurrentSwim, continueCurrentSwim,
                   RESOURCE_ID_IMAGE_ACTION_ICON_OK, RESOURCE_ID_IMAGE_ACTION_ICON_NOK);
}

static void onDigitActionBarLayerSelectClicked(ClickRecognizerRef recognizer, void * context)
{
    setPaused(!isPaused());
}

static void updateLaneDigits()
{
    ClockDigit_setNumber(&clockDigits[0], (laneCount/ 10) % 10, FONT_SETTING_DEFAULT);
    ClockDigit_setNumber(&clockDigits[1],  laneCount      % 10, FONT_SETTING_DEFAULT);
}

static void continueCurrentSwim()
{
    setPaused(false);
}

static void continueCurrentSwimInNextLane()
{
    continueCurrentSwim();
    startNextLane();
}

static void updateTimeDigits()
{
    const time_t now = time(NULL);
    int remaining = isPaused() ? virtualEndTimeOfCurrentLane + (now - startTimeOfCurrentPause) - now
                               : virtualEndTimeOfCurrentLane - now;

    if (remaining <= 0 && !isPaused()) {
        if (laneCount == desiredLaneCount) {
            setPaused(true);
            showMessageBox("Yeah, workout finished :-). Quit swim?", quitCurrentSwim, continueCurrentSwimInNextLane,
                           RESOURCE_ID_IMAGE_ACTION_ICON_OK, RESOURCE_ID_IMAGE_ACTION_ICON_NOK);
            return;
        }
        startNextLane();
        return;
    }

    ClockDigit_setNumber(&clockDigits[2], (remaining / 10) % 10, FONT_SETTING_BOLD);
    ClockDigit_setNumber(&clockDigits[3],  remaining       % 10, FONT_SETTING_BOLD);

    if (remaining <= 3 && !isPaused()) {
        if (laneCount == desiredLaneCount) {
            vibes_long_pulse();
        } else {
            vibes_short_pulse();
        }
    }
}

static void startNextLane()
{
    ++laneCount;

    const time_t now = time(NULL);

    // calculate next timePerLane
    finishLane();

    // start new lane
    startTimeOfCurrentLane = now;
    cumulatedPauseTimeOfCurrentLane = 0;

    if (isPaused()) {
        startTimeOfCurrentPause = now;
    } else {
        virtualEndTimeOfCurrentLane = startTimeOfCurrentLane + timePerLane;
    }

    vibes_long_pulse();
    updateLaneDigits();
    updateTimeDigits();
}

static void finishLane()
{
    const time_t now = time(NULL);

    // calculate next timePerLane
    if (isPaused()) {
        cumulatedPauseTimeOfWorkout     += now - startTimeOfCurrentPause;
        cumulatedPauseTimeOfCurrentLane += now - startTimeOfCurrentPause;
    }
    if (startTimeOfCurrentLane > 0) {
        timePerLane = now - startTimeOfCurrentLane - cumulatedPauseTimeOfCurrentLane;
        timeOfPreviousLane = timePerLane;
    }
}

static void restartCurrentLane()
{
    const time_t now = time(NULL);

    // calculate next timePerLane
    finishLane();

    // start new lane
    startTimeOfCurrentLane = now;
    cumulatedPauseTimeOfCurrentLane = 0;

    if (isPaused()) {
        startTimeOfCurrentPause = now;
    } else {
        virtualEndTimeOfCurrentLane = startTimeOfCurrentLane + timePerLane;
    }

    vibes_long_pulse();
    updateLaneDigits();
    updateTimeDigits();
}

static void onDigitActionBarLayerDownClicked(ClickRecognizerRef recognizer, void * context)
{
    startNextLane();
}

static void onDigitActionBarLayerDownDoubleClicked(ClickRecognizerRef recognizer, void * context)
{
    restartCurrentLane();
}

static void digitActionBarLayerClickConfigProvider(void * context)
{
    window_single_click_subscribe(BUTTON_ID_BACK,   (ClickHandler)onDigitActionBarLayerBackClicked);
    window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler)onDigitActionBarLayerSelectClicked);
    window_single_click_subscribe(BUTTON_ID_DOWN,   (ClickHandler)onDigitActionBarLayerDownClicked);
    window_multi_click_subscribe(BUTTON_ID_DOWN, 2, 0, 0, true, (ClickHandler)onDigitActionBarLayerDownDoubleClicked);
}

static void handleSecondsTick(struct tm * tick_time, TimeUnits units_changed)
{
    updateTimeDigits();
}

static void onDigitWindowLoad(Window * window)
{
    GPoint digitPoints[4] = {GPoint(7, 7), GPoint(60, 7), GPoint(7, 90), GPoint(60, 90)};

    for(int i = 0; i < 4; i++) {
        ClockDigit_construct(&clockDigits[i], digitPoints[i]);
    }

    Layer * windowRootLayer = window_get_root_layer(window);
    for(int i = 0; i < 4; i++) {
        //ClockDigit_setColor(&clockDigits[i], GColorBlack, GColorWhite);
        layer_add_child(windowRootLayer, bitmap_layer_get_layer(clockDigits[i].imageLayer));
    }

    // Initialize the action bar:
    digitActionBarLayer = action_bar_layer_create();
    action_bar_layer_set_click_config_provider(digitActionBarLayer, digitActionBarLayerClickConfigProvider);

    updateDigitActionBarLayerIcons();

    action_bar_layer_add_to_window(digitActionBarLayer, window);

    // reset all
    laneCount = 0;
    startTimeOfWorkout = time(NULL);
    timeOfPreviousLane = timePerLane;
    startTimeOfCurrentLane  = 0;
    cumulatedPauseTimeOfWorkout = 0;
    cumulatedPauseTimeOfCurrentLane = 0;
    startTimeOfCurrentPause = 0;
    virtualEndTimeOfCurrentLane = 0;
    startNextLane();

    tick_timer_service_subscribe(SECOND_UNIT, &handleSecondsTick);
}

static void onDigitWindowUnload(Window * window)
{
    tick_timer_service_unsubscribe();

    action_bar_layer_destroy(digitActionBarLayer);
    digitActionBarLayer = NULL;

    for(int i = 0; i < 4; i++) {
        ClockDigit_destruct(&clockDigits[i]);
    }
}

static void initDigitWindow()
{
    digitWindow = window_create();
    window_set_window_handlers(digitWindow, (WindowHandlers){
                                   .load   = onDigitWindowLoad,
                                   .unload = onDigitWindowUnload,
                               });
}

static void deinitDigitWindow()
{
    window_destroy(digitWindow);
    digitWindow = NULL;
}

//
// SummaryMenuLayer

static uint16_t onSummaryMenuGetNumRows(MenuLayer * menuLayer, uint16_t sectionIndex, void * data)
{
    switch (sectionIndex) {
    case 0:  return NUM_SUMMARY_MENU_ITEMS;
    default: return 0;
    }
}

static void formtTime(char * str, size_t maxlen, time_t time)
{
    const int hours =  time / 60 / 60;
    const int mins  = (time / 60) % 60;
    const int secs  =  time % 60;

    if (hours == 0) {
        snprintf(str, maxlen, "%d%d:%d%d",
                 mins >= 10 ? mins / 10 : 0,
                 mins % 10,
                 secs >= 10 ? secs / 10 : 0,
                 secs % 10
                 );
    } else {
        snprintf(str, maxlen, "%d%d:%d%d:%d%d",
                 hours >= 10 ? hours / 10 : 0,
                 hours % 10,
                 mins >= 10 ? mins / 10 : 0,
                 mins % 10,
                 secs >= 10 ? secs / 10 : 0,
                 secs % 10
                 );    }
}

static void onSummaryMenuDrawRow(GContext* ctx, const Layer * cellLayer, MenuIndex * cellIndex, void * data)
{
    const int swimTime = lastWorkoutEndTimeOfWorkout - lastWorkoutStartTimeOfWorkout - lastWorkoutCumulatedPauseTimeOfWorkout;
    const int avgTimePerLane = swimTime / lastWorkoutLaneCount;

    switch (cellIndex->section) {
    case 0:
        switch (cellIndex->row) {
        case 0: {
            struct tm * tmTime = localtime(&lastWorkoutStartTimeOfWorkout);
            char str[20];
            strftime(str, 20, "%d.%m.%Y %H:%M:%S", tmTime);
            menu_cell_basic_draw(ctx, cellLayer, "Start of swim", str, NULL);
            break;
        }
        case 1: {
            char str[10];
            formtTime(str, 10, swimTime);
            menu_cell_basic_draw(ctx, cellLayer, "Total swim time", str, NULL);
            break;
        }
        case 2: {
            char str[10];
            formtTime(str, 10, avgTimePerLane);
            menu_cell_basic_draw(ctx, cellLayer, "Time per Lane", str, NULL);
            break;
        }
        case 3: {
            char str[12];
            snprintf(str, 12, "%d (%dm)", lastWorkoutLaneCount, lastWorkoutLaneCount*lastWorkoutLengthOfLane);
            menu_cell_basic_draw(ctx, cellLayer, "Lanes", str, NULL);
            break;
        }
        case 4: {
            char str[10];
            formtTime(str, 10, lastWorkoutCumulatedPauseTimeOfWorkout);
            menu_cell_basic_draw(ctx, cellLayer, "Pause", str, NULL);
            break;
        }
        case 5: {
            struct tm * tmTime = localtime(&lastWorkoutEndTimeOfWorkout);
            char str[20];
            strftime(str, 20, "%d.%m.%Y %H:%M:%S", tmTime);
            menu_cell_basic_draw(ctx, cellLayer, "End of swim", str, NULL);
            break;
        }
        }
    }
}

static void onSummaryMenuWindowLoad(Window * window)
{
    // Now we prepare to initialize the menu layer
    Layer * windowRootLayer = window_get_root_layer(window);
    const GRect bounds = layer_get_frame(windowRootLayer);

    // Create the menu layer
    summaryMenuLayer = menu_layer_create(bounds);
    menu_layer_set_callbacks(summaryMenuLayer, NULL, (MenuLayerCallbacks){
                                 .get_num_sections  = NULL,
                                 .get_num_rows      = onSummaryMenuGetNumRows,
                                 .get_header_height = NULL,
                                 .draw_header       = NULL,
                                 .draw_row          = onSummaryMenuDrawRow,
                                 .select_click      = NULL,
                                 .get_cell_height   = NULL,
                             });

    menu_layer_set_click_config_onto_window(summaryMenuLayer, window);

    layer_add_child(windowRootLayer, menu_layer_get_layer(summaryMenuLayer));
}

static void onSummaryMenuWindowUnload(Window * window)
{
    menu_layer_destroy(summaryMenuLayer);
    summaryMenuLayer = NULL;
}

static void initSummaryMenuWindow()
{
    summaryMenuWindow = window_create();
    window_set_window_handlers(summaryMenuWindow, (WindowHandlers){
                                   .load   = onSummaryMenuWindowLoad,
                                   .unload = onSummaryMenuWindowUnload,
                               });
}

static void deinitSummaryMenuWindow()
{
    window_destroy(summaryMenuWindow);
}

//
// MenuLayer

static uint16_t onMainMenuGetNumSections(MenuLayer * menuLayer, void * data)
{
    return NUM_MENU_SECTIONS;
}

static uint16_t onMainMenuGetNumRows(MenuLayer * menuLayer, uint16_t sectionIndex, void * data)
{
    switch (sectionIndex) {
    case 0:  return NUM_1ST_MENU_ITEMS;
    case 1:  return NUM_2ND_MENU_ITEMS;
    case 2:  return NUM_3RD_MENU_ITEMS;
    default: return 0;
    }
}

static int16_t onMainMenuGetHeaderHeight(MenuLayer * menuLayer, uint16_t sectionIndex, void * data)
{
    return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void onMainMenuDrawHeader(GContext * ctx, const Layer * cellLayer, uint16_t sectionIndex, void * data)
{
    switch (sectionIndex) {
    case 0:
        menu_cell_basic_header_draw(ctx, cellLayer, "New swim");
        break;
    case 1:
        menu_cell_basic_header_draw(ctx, cellLayer, "Last Workout");
        break;
    case 2:
        menu_cell_basic_header_draw(ctx, cellLayer, "Setup");
        break;
    }
}

static void onMainMenuDrawRow(GContext* ctx, const Layer * cellLayer, MenuIndex * cellIndex, void * data)
{
    switch (cellIndex->section) {
    case 0:
        switch (cellIndex->row) {
        case 0: {
            char str[12];
            snprintf(str, 12, "%d (%dm)", desiredLaneCount, desiredLaneCount*lengthOfLane);
            menu_cell_basic_draw(ctx, cellLayer, "Desired lanes", str, NULL);
            break;
        }
        case 1: {
            char str[5];
            snprintf(str, 5, "%ds", timePerLane);
            menu_cell_basic_draw(ctx, cellLayer, "Time per lane", str, NULL);
            break;
        }
        case 2:
            menu_cell_basic_draw(ctx, cellLayer, "Start", NULL, NULL);
            break;
        }
        break;
    case 1:
        switch (cellIndex->row) {
        case 0: {
            menu_cell_basic_draw(ctx, cellLayer, "Show last swim", NULL, NULL);
            break;
        }
        }
        break;
    case 2:
        switch (cellIndex->row) {
        case 0: {
            char str[4];
            snprintf(str, 4, "%dm", lengthOfLane);
            menu_cell_basic_draw(ctx, cellLayer, "Length of lane", str, NULL);
            break;
        }
        }
        break;
    }
}

static void onMainMenuMenuSelect(MenuLayer * menuLayer, MenuIndex * cellIndex, void * data)
{
    switch (cellIndex->section) {
    case 0:
        switch (cellIndex->row) {
        case 0:
            currentValueToChange = &desiredLaneCount;
            action_bar_layer_add_to_window(actionBarLayer, mainMenuWindow);
            break;
        case 1:
            currentValueToChange = &timePerLane;
            action_bar_layer_add_to_window(actionBarLayer, mainMenuWindow);
            break;
        case 2:
            window_stack_push(digitWindow, true);
            break;
        }
        break;
    case 1:
        switch (cellIndex->row) {
        case 0:
            window_stack_push(summaryMenuWindow, true);
            break;
        }
        break;
    case 2:
        switch (cellIndex->row) {
        case 0:
            if (lengthOfLane == 25) lengthOfLane = 50;
            else                    lengthOfLane = 25;
            layer_mark_dirty(menu_layer_get_layer(menuLayer));
            break;
        }
        break;
    }
}

//
// ActionBar Layer

static void onActionBarLayerBackClicked(ClickRecognizerRef recognizer, void * context)
{
    currentValueToChange = NULL;
    action_bar_layer_remove_from_window(actionBarLayer);
    setClickContextProviderForMainMenu(mainMenuLayer, mainMenuWindow);
}

static void onActionBarLayerUpClicked(ClickRecognizerRef recognizer, void * context)
{
    if (*currentValueToChange >= 999) return;
    ++(*currentValueToChange);
    layer_mark_dirty(menu_layer_get_layer(mainMenuLayer));
}

static void onActionBarLayerDownClicked(ClickRecognizerRef recognizer, void * context)
{
    if (*currentValueToChange <= 0) return;
    --(*currentValueToChange);
    layer_mark_dirty(menu_layer_get_layer(mainMenuLayer));
}

static void actionBarLayerClickConfigProvider(void * context)
{
    window_single_click_subscribe(BUTTON_ID_BACK,                (ClickHandler)onActionBarLayerBackClicked);
    window_single_click_subscribe(BUTTON_ID_SELECT,              (ClickHandler)onActionBarLayerBackClicked);
    window_single_click_subscribe(BUTTON_ID_UP,                  (ClickHandler)onActionBarLayerUpClicked);
    window_single_click_subscribe(BUTTON_ID_DOWN,                (ClickHandler)onActionBarLayerDownClicked);
    window_single_repeating_click_subscribe(BUTTON_ID_UP,   200, (ClickHandler)onActionBarLayerUpClicked);
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, (ClickHandler)onActionBarLayerDownClicked);
}

//
// main window

static void quit()
{
    window_stack_pop_all(false);
}

static void onBackClicked(ClickRecognizerRef recognizer, void * context)
{
    showMessageBox("Already done?", quit, 0,
                   RESOURCE_ID_IMAGE_ACTION_ICON_OK,
                   RESOURCE_ID_IMAGE_ACTION_ICON_NOK);
}

// We need to save a reference to the ClickConfigProvider originally set by the menu layer
ClickConfigProvider mainMenuClickContextProvider;

void mainMenuWindowClickContextProvider(void * context)
{
    mainMenuClickContextProvider(context);
    window_single_click_subscribe(BUTTON_ID_BACK, onBackClicked);
}

void setClickContextProviderForMainMenu(MenuLayer * menuLayer, Window * window)
{
    // Bind the menu layer's click config provider to the window for interactivity
    menu_layer_set_click_config_onto_window(menuLayer, window);

    mainMenuClickContextProvider = window_get_click_config_provider(window);
    window_set_click_config_provider_with_context(window, mainMenuWindowClickContextProvider, menuLayer);
}

static void onMainMenuWindowLoad(Window * window)
{
    // Now we prepare to initialize the menu layer
    Layer * windowRootLayer = window_get_root_layer(window);
    const GRect bounds = layer_get_frame(windowRootLayer);

    // Create the menu layer
    mainMenuLayer = menu_layer_create(bounds);
    menu_layer_set_callbacks(mainMenuLayer, NULL, (MenuLayerCallbacks){
                                 .get_num_sections  = onMainMenuGetNumSections,
                                 .get_num_rows      = onMainMenuGetNumRows,
                                 .get_header_height = onMainMenuGetHeaderHeight,
                                 .draw_header       = onMainMenuDrawHeader,
                                 .draw_row          = onMainMenuDrawRow,
                                 .select_click      = onMainMenuMenuSelect,
                                 .get_cell_height   = NULL,
                             });

    setClickContextProviderForMainMenu(mainMenuLayer, window);

    layer_add_child(windowRootLayer, menu_layer_get_layer(mainMenuLayer));

    // Initialize the action bar:
    actionBarLayer = action_bar_layer_create();
    action_bar_layer_set_click_config_provider(actionBarLayer, actionBarLayerClickConfigProvider);

    action_bar_layer_set_icon_animated(actionBarLayer, BUTTON_ID_UP,     iconUp,   true);
    action_bar_layer_set_icon_animated(actionBarLayer, BUTTON_ID_SELECT, iconOK,   true);
    action_bar_layer_set_icon_animated(actionBarLayer, BUTTON_ID_DOWN,   iconDown, true);
}

static void onMainMenuWindowUnload(Window * window)
{
    menu_layer_destroy(mainMenuLayer);
    mainMenuLayer = NULL;

    action_bar_layer_destroy(actionBarLayer);
    actionBarLayer = NULL;
}

static void initMainMenuWindow()
{
    mainMenuWindow = window_create();
    window_set_window_handlers(mainMenuWindow, (WindowHandlers){
                                   .load   = onMainMenuWindowLoad,
                                   .unload = onMainMenuWindowUnload,
                               });
    window_stack_push(mainMenuWindow, true);
}

static void deinitMainMenuWindow()
{
    window_destroy(mainMenuWindow);
}

#define readPersistInt(key, variable) \
    do { if (persist_exists(key)) variable = persist_read_int(key); } while (0)

static void readPersistentSettings()
{
    readPersistInt(PERSIST_KEY_DESIRED_LANE_COUNT,            desiredLaneCount);
    readPersistInt(PERSIST_KEY_TIME_PER_LANE,                 timePerLane);
    readPersistInt(PERSIST_KEY_LENGTH_OF_LANE,                lengthOfLane);
    readPersistInt(PERSIST_KEY_LAST_WORKOUT_LENGTH_OF_LANE,   lastWorkoutLengthOfLane);
    readPersistInt(PERSIST_KEY_LAST_WORKOUT_LANE_COUNT,       lastWorkoutLaneCount);
    readPersistInt(PERSIST_KEY_LAST_WORKOUT_START_OF_WORKOUT, lastWorkoutStartTimeOfWorkout);
    readPersistInt(PERSIST_KEY_LAST_WORKOUT_CUMULATIVE_PAUSE, lastWorkoutCumulatedPauseTimeOfWorkout);
    readPersistInt(PERSIST_KEY_LAST_WORKOUT_END_OF_WORKOUT,   lastWorkoutEndTimeOfWorkout);
}

static void writePersistentSettings()
{
    persist_write_int(PERSIST_KEY_DESIRED_LANE_COUNT,            desiredLaneCount);
    persist_write_int(PERSIST_KEY_TIME_PER_LANE,                 timePerLane);
    persist_write_int(PERSIST_KEY_LENGTH_OF_LANE,                lengthOfLane);
    persist_write_int(PERSIST_KEY_LAST_WORKOUT_LENGTH_OF_LANE,   lastWorkoutLengthOfLane);
    persist_write_int(PERSIST_KEY_LAST_WORKOUT_LANE_COUNT,       lastWorkoutLaneCount);
    persist_write_int(PERSIST_KEY_LAST_WORKOUT_START_OF_WORKOUT, lastWorkoutStartTimeOfWorkout);
    persist_write_int(PERSIST_KEY_LAST_WORKOUT_CUMULATIVE_PAUSE, lastWorkoutCumulatedPauseTimeOfWorkout);
    persist_write_int(PERSIST_KEY_LAST_WORKOUT_END_OF_WORKOUT,   lastWorkoutEndTimeOfWorkout);
}

static void initIcons()
{
    iconUp    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_UP);
    iconOK    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_OK);
    iconDown  = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_DOWN);
    iconPlay  = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_PLAY);
    iconPause = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_PAUSE);
}

static void deinitIcons()
{
    gbitmap_destroy(iconUp);
    gbitmap_destroy(iconOK);
    gbitmap_destroy(iconDown);
    gbitmap_destroy(iconPlay);
    gbitmap_destroy(iconPause);

    iconUp    = NULL;
    iconOK    = NULL;
    iconDown  = NULL;
    iconPlay  = NULL;
    iconPause = NULL;
}

int main(void)
{
    readPersistentSettings();
    initIcons();

    initMainMenuWindow();
    initDigitWindow();
    initSummaryMenuWindow();

    app_event_loop();

    deinitSummaryMenuWindow();
    deinitDigitWindow();
    deinitMainMenuWindow();

    deinitIcons();
    writePersistentSettings();
}
