#include "pebble.h"

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

// forward declarations
void setClickContextProviderForMainMenu(MenuLayer * menuLayer, Window * window);

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

static void click_config_provider(void * context)
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
    action_bar_layer_set_click_config_provider(actionBarLayer, click_config_provider);

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
    iconUp   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_UP);
    iconOK   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_OK);
    iconDown = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_DOWN);
}

static void deinitIcons()
{
    gbitmap_destroy(iconUp);
    gbitmap_destroy(iconOK);
    gbitmap_destroy(iconDown);

    iconUp   = NULL;
    iconOK   = NULL;
    iconDown = NULL;
}

int main(void)
{
    readPersistentSettings();
    initIcons();

    initMainMenuWindow();
    app_event_loop();
    deinitMainMenuWindow();

    deinitIcons();
    writePersistentSettings();
}
