#include "pebble.h"

#include "clock_digit.h"
#include "messagebox.h"

// main menu stuff
#define NUM_MENU_SECTIONS 2
#define NUM_FIRST_MENU_ITEMS 3
#define NUM_SECOND_MENU_ITEMS 1

// settings keys
#define PERSIST_KEY_DESIRED_LANE_COUNT 0
#define PERSIST_KEY_TIME_PER_LANE      1
#define PERSIST_KEY_LENGTH_OF_LANE     2

// settings variables
static int desiredLaneCount = 40;
static int timePerLane      = 36;
static int lengthOfLane     = 25;
int * currentValueToChange = NULL;

// layers
static Window * mainMenuWindow;
static MenuLayer * mainMenuLayer;
static ActionBarLayer *actionBarLayer;

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
time_t startTimeOfCurrentLane  = 0;
int cumulatedPauseTime = 0;
time_t startTimeOfCurrentPause = 0;
time_t virtualEndTimeOfCurrentLane = 0;

// forward declarations
static void finishLane();
static void startNextLane();
void setClickContextProviderForMainMenu(MenuLayer * menuLayer, Window * window);

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

static void onDigitActionBarLayerSelectClicked(ClickRecognizerRef recognizer, void * context)
{
    const bool isNowPaused = !isPaused();

    const time_t now = time(NULL);
    if (isNowPaused) {
        startTimeOfCurrentPause = now;
    } else {
        cumulatedPauseTime += now - startTimeOfCurrentPause;
        startTimeOfCurrentPause = 0;

        virtualEndTimeOfCurrentLane = startTimeOfCurrentLane + timePerLane + cumulatedPauseTime;
    }

    updateDigitActionBarLayerIcons();
}

static void updateLaneDigits()
{
    ClockDigit_setNumber(&clockDigits[0], (laneCount/ 10) % 10, FONT_SETTING_DEFAULT);
    ClockDigit_setNumber(&clockDigits[1],  laneCount      % 10, FONT_SETTING_DEFAULT);
}

static void updateTimeDigits()
{
    const time_t now = time(NULL);
    int remaining = isPaused() ? virtualEndTimeOfCurrentLane + (now - startTimeOfCurrentPause) - now
                               : virtualEndTimeOfCurrentLane - now;

    if (remaining <= 0 && !isPaused()) {
        startNextLane();
        return;
    }

    ClockDigit_setNumber(&clockDigits[2], (remaining / 10) % 10, FONT_SETTING_BOLD);
    ClockDigit_setNumber(&clockDigits[3],  remaining       % 10, FONT_SETTING_BOLD);

    if (remaining <= 3 && !isPaused()) {
        vibes_short_pulse();
    }
}

static void finishLane()
{
    const time_t now = time(NULL);

    // calculate next timePerLane
    if (isPaused()) {
        cumulatedPauseTime += now - startTimeOfCurrentPause;
    }
    if (startTimeOfCurrentLane > 0) {
        timePerLane = now - startTimeOfCurrentLane - cumulatedPauseTime;
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
    cumulatedPauseTime = 0;

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

static void digitActionBarLayerClickConfigProvider(void * context)
{
    window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler)onDigitActionBarLayerSelectClicked);
    window_single_click_subscribe(BUTTON_ID_DOWN,   (ClickHandler)onDigitActionBarLayerDownClicked);
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

    if (!isPaused()) {
        startNextLane();
    }
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
// MenuLayer

static uint16_t onMainMenuGetNumSections(MenuLayer * menuLayer, void * data)
{
    return NUM_MENU_SECTIONS;
}

static uint16_t onMainMenuGetNumRows(MenuLayer * menuLayer, uint16_t sectionIndex, void * data)
{
    switch (sectionIndex) {
    case 0:  return NUM_FIRST_MENU_ITEMS;
    case 1:  return NUM_SECOND_MENU_ITEMS;
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
            char str[4];
            snprintf(str, 4, "%dm", lengthOfLane);
            menu_cell_basic_draw(ctx, cellLayer, "Length of lane", str, NULL);
            break;
        }
        }
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

static void readPersistentSettings()
{
    if (persist_exists(PERSIST_KEY_DESIRED_LANE_COUNT)) {
        desiredLaneCount = persist_read_int(PERSIST_KEY_DESIRED_LANE_COUNT);
    }
    if (persist_exists(PERSIST_KEY_TIME_PER_LANE)) {
        timePerLane = persist_read_int(PERSIST_KEY_TIME_PER_LANE);
    }
    if (persist_exists(PERSIST_KEY_LENGTH_OF_LANE)) {
        lengthOfLane = persist_read_int(PERSIST_KEY_LENGTH_OF_LANE);
    }
}

static void writePersistentSettings()
{
    persist_write_int(PERSIST_KEY_DESIRED_LANE_COUNT, desiredLaneCount);
    persist_write_int(PERSIST_KEY_TIME_PER_LANE,      timePerLane);
    persist_write_int(PERSIST_KEY_LENGTH_OF_LANE,     lengthOfLane);
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

    app_event_loop();

    deinitDigitWindow();
    deinitMainMenuWindow();

    deinitIcons();
    writePersistentSettings();
}
