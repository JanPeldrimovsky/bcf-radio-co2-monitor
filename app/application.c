#include <application.h>

#define SECOND (1000)
#define MINUTE (60 * SECOND)
#define HOUR (60 * MINUTE)

#define SERVICE_INTERVAL_INTERVAL (HOUR)
#define BATTERY_UPDATE_INTERVAL (HOUR)

#define TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL (15 * MINUTE)
#define TEMPERATURE_TAG_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (5 * MINUTE)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL (10 * MINUTE)

#define VOC_TAG_LP_PUB_NO_CHANGE_INTERVAL (15 * MINUTE) 
#define VOC_TAG_LP_PUB_VALUE_CHANGE 50.0f
#define VOC_TAG_LP_UPDATE_SERVICE_INTERVAL (1 * MINUTE)
#define VOC_TAG_LP_UPDATE_NORMAL_INTERVAL (5 * MINUTE)

#define HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL (15 * MINUTE)
#define HUMIDITY_TAG_PUB_VALUE_CHANGE 5.0f
#define HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL (5 * MINUTE)
#define HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL (10 * MINUTE)

#define BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL (15 * MINUTE)
#define BAROMETER_TAG_PUB_VALUE_CHANGE 20.0f
#define BAROMETER_TAG_UPDATE_SERVICE_INTERVAL (1 * MINUTE)
#define BAROMETER_TAG_UPDATE_NORMAL_INTERVAL  (5 * MINUTE)

#define CO2_PUB_NO_CHANGE_INTERVAL (15 * MINUTE)
#define CO2_PUB_VALUE_CHANGE 50.0f
#define CO2_UPDATE_SERVICE_INTERVAL (1 * MINUTE)
#define CO2_UPDATE_NORMAL_INTERVAL  (5 * MINUTE)

#define CALIBRATION_DELAY (4 * MINUTE)
#define CALIBRATION_INTERVAL (1 * MINUTE)

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

// Thermometer instance
bc_tmp112_t tmp112;
event_param_t temperature_event_param = { .next_pub = 0 };

// VOC tag instance
bc_tag_voc_lp_t voc;
event_param_t voc_event_param = { .next_pub = 0 };

// Humidity tag instance
bc_tag_humidity_t humidity;
event_param_t humidity_event_param = { .next_pub = 0 };

// Barometer tag instance
bc_tag_barometer_t barometer;
event_param_t barometer_event_param = { .next_pub = 0 };

// CO2
event_param_t co2_event_param = { .next_pub = 0 };

bc_scheduler_task_id_t calibration_task_id = 0;
int calibration_counter;

void calibration_task(void *param);

void calibration_start()
{
    calibration_counter = 32;

    bc_led_set_mode(&led, BC_LED_MODE_BLINK_FAST);
    calibration_task_id = bc_scheduler_register(calibration_task, NULL, bc_tick_get() + CALIBRATION_DELAY);
    bc_radio_pub_string("co2-meter/-/calibration", "start");
}

void calibration_stop()
{
    bc_led_set_mode(&led, BC_LED_MODE_OFF);
    bc_scheduler_unregister(calibration_task_id);
    calibration_task_id = 0;

    bc_module_co2_set_update_interval(CO2_UPDATE_NORMAL_INTERVAL);
    bc_radio_pub_string("co2-meter/-/calibration", "end");
}

void calibration_task(void *param)
{
    (void) param;

    bc_led_set_mode(&led, BC_LED_MODE_BLINK_SLOW);

    bc_radio_pub_int("co2-meter/-/calibration", &calibration_counter);

    bc_module_co2_set_update_interval(CO2_UPDATE_SERVICE_INTERVAL);
    bc_module_co2_calibration(BC_LP8_CALIBRATION_BACKGROUND_FILTERED);

    calibration_counter--;

    if (calibration_counter == 0)
    {
        calibration_stop();
    }

    bc_scheduler_plan_current_relative(CALIBRATION_INTERVAL);
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_CLICK)
    {
        static uint16_t button_count = 0;

        bc_led_pulse(&led, 100);

        bc_radio_pub_push_button(&button_count);
        button_count++;
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        if (!calibration_task_id)
        {
            calibration_start();
        }
        else
        {
            calibration_stop();
        }

    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_radio_pub_battery(&voltage);
        }
    }
}

void temperature_tag_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        if (bc_tmp112_get_temperature_celsius(self, &value))
        {
            if ((fabsf(value - param->value) >= TEMPERATURE_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_temperature(param->channel, &value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void voc_tag_event_handler(bc_tag_voc_lp_t *self, bc_tag_voc_lp_event_t event, void *event_param)
{
    uint16_t value;
    int int_value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TAG_VOC_LP_EVENT_UPDATE)
    {
        if (bc_tag_voc_lp_get_tvoc_ppb(self, &value))
        {
            if ((fabsf(value - param->value) >= VOC_TAG_LP_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                int_value = value;
                bc_radio_pub_int("voc-sensor/0:0/tvoc", &int_value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + VOC_TAG_LP_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_HUMIDITY_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_humidity_get_humidity_percentage(self, &value))
    {
        if ((fabsf(value - param->value) >= HUMIDITY_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
            bc_radio_pub_humidity(param->channel, &value);
            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL;
        }
    }
}

void barometer_tag_event_handler(bc_tag_barometer_t *self, bc_tag_barometer_event_t event, void *event_param)
{
    float pascal;
    float meter;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_BAROMETER_EVENT_UPDATE)
    {
        return;
    }

    if (!bc_tag_barometer_get_pressure_pascal(self, &pascal))
    {
        return;
    }

    if ((fabsf(pascal - param->value) >= BAROMETER_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
    {

        if (!bc_tag_barometer_get_altitude_meter(self, &meter))
        {
            return;
        }

        bc_radio_pub_barometer(param->channel, &pascal, &meter);
        param->value = pascal;
        param->next_pub = bc_scheduler_get_spin_tick() + BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL;
    }
}

void co2_event_handler(bc_module_co2_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *) event_param;
    float value;

    if (event == BC_MODULE_CO2_EVENT_ERROR)
    {
        bc_lp8_error_t error;
        bc_module_co2_get_error(&error);
        int error_int = (int)error;

        bc_radio_pub_int("co2-meter/-/error", &error_int);
    }

    if (event == BC_MODULE_CO2_EVENT_UPDATE)
    {
        if (bc_module_co2_get_concentration_ppm(&value))
        {
            if ((fabsf(value - param->value) >= CO2_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()) || calibration_task_id)
            {
                bc_radio_pub_co2(&value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + CO2_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void switch_to_normal_mode_task(void *param)
{
    bc_tag_voc_lp_set_update_interval(&voc, VOC_TAG_LP_UPDATE_NORMAL_INTERVAL);

    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_NORMAL_INTERVAL);

    bc_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL);

    bc_tag_barometer_set_update_interval(&barometer, BAROMETER_TAG_UPDATE_NORMAL_INTERVAL);

    bc_module_co2_set_update_interval(CO2_UPDATE_NORMAL_INTERVAL);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

void application_init(void)
{
    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);

    // Initialize temperature
    temperature_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_UPDATE_SERVICE_INTERVAL);
    bc_tmp112_set_event_handler(&tmp112, temperature_tag_event_handler, &temperature_event_param);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_hold_time(&button, 10000);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize voc
    voc_event_param.channel = BC_RADIO_PUB_CHANNEL_R2_I2C0_ADDRESS_DEFAULT;
    bc_tag_voc_lp_init(&voc, BC_I2C_I2C0);
    bc_tag_voc_lp_set_update_interval(&voc, VOC_TAG_LP_UPDATE_SERVICE_INTERVAL);
    bc_tag_voc_lp_set_event_handler(&voc, voc_tag_event_handler, &voc_event_param);

    // Initialize humidity
    humidity_event_param.channel = BC_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT;
    bc_tag_humidity_init(&humidity, BC_TAG_HUMIDITY_REVISION_R3, BC_I2C_I2C0, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_humidity_set_event_handler(&humidity, humidity_tag_event_handler, &humidity_event_param);

    // Initialize barometer
    barometer_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    bc_tag_barometer_init(&barometer, BC_I2C_I2C0);
    bc_tag_barometer_set_update_interval(&barometer, BAROMETER_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_barometer_set_event_handler(&barometer, barometer_tag_event_handler, &barometer_event_param);

    // Initialize CO2
    bc_module_co2_init();
    bc_module_co2_set_update_interval(CO2_UPDATE_SERVICE_INTERVAL);
    bc_module_co2_set_event_handler(co2_event_handler, &co2_event_param);

    bc_radio_pairing_request("co2-monitor", VERSION);

    bc_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);

    bc_led_pulse(&led, 2000);
}
