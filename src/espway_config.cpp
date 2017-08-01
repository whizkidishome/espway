extern "C" {
#include <sysparam.h>
}

#include "espway.h"

espway_config my_config;

const static espway_config DEFAULT_CONFIG = {
    .pid_coeffs_arr = {
        { FLT_TO_Q16(ANGLE_KP), FLT_TO_Q16(ANGLE_KI), FLT_TO_Q16(ANGLE_KD) },
        { FLT_TO_Q16(ANGLE_HIGH_KP), FLT_TO_Q16(ANGLE_HIGH_KI), FLT_TO_Q16(ANGLE_HIGH_KD) },
        { FLT_TO_Q16(VEL_KP), FLT_TO_Q16(VEL_KI), FLT_TO_Q16(VEL_KD) }
    },
    .gyro_offsets = { GYRO_X_OFFSET, GYRO_Y_OFFSET, GYRO_Z_OFFSET }
};

void pretty_print_config() {
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    printf(
        "\n\nESPway current config:\n\n"
        "#define ANGLE_KP %d\n"
        "#define ANGLE_KI %d\n"
        "#define ANGLE_KD %d\n"
        "#define ANGLE_HIGH_KP %d\n"
        "#define ANGLE_HIGH_KI %d\n"
        "#define ANGLE_HIGH_KD %d\n"
        "#define VEL_KP %d\n"
        "#define VEL_KI %d\n"
        "#define VEL_KD %d\n"
        "#define GYRO_X_OFFSET %d\n"
        "#define GYRO_Y_OFFSET %d\n"
        "#define GYRO_Z_OFFSET %d\n"
        "\n\n",
        my_config.pid_coeffs_arr[ANGLE].p,
        my_config.pid_coeffs_arr[ANGLE].i,
        my_config.pid_coeffs_arr[ANGLE].d,
        my_config.pid_coeffs_arr[ANGLE_HIGH].p,
        my_config.pid_coeffs_arr[ANGLE_HIGH].i,
        my_config.pid_coeffs_arr[ANGLE_HIGH].d,
        my_config.pid_coeffs_arr[VEL].p,
        my_config.pid_coeffs_arr[VEL].i,
        my_config.pid_coeffs_arr[VEL].d,
        my_config.gyro_offsets[0],
        my_config.gyro_offsets[1],
        my_config.gyro_offsets[2]
    );
    xSemaphoreGive(pid_mutex);
}

void load_hardcoded_config() {
    my_config = DEFAULT_CONFIG;
}

void load_stored_config() {
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    load_hardcoded_config();
    sysparam_get_data_static("ANGLE_PID", (uint8_t *)&my_config.pid_coeffs_arr[ANGLE],
        sizeof(pid_coeffs), NULL, NULL);
    sysparam_get_data_static("ANGLE_HIGH_PID", (uint8_t *)&my_config.pid_coeffs_arr[ANGLE_HIGH],
        sizeof(pid_coeffs), NULL, NULL);
    sysparam_get_data_static("VEL_PID", (uint8_t *)&my_config.pid_coeffs_arr[VEL],
        sizeof(pid_coeffs), NULL, NULL);
    sysparam_get_data_static("GYRO_OFFSETS", (uint8_t *)&my_config.gyro_offsets,
        3 * sizeof(int16_t), NULL, NULL);
    xSemaphoreGive(pid_mutex);
}

void apply_config_params() {
    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    pid_initialize(&my_config.pid_coeffs_arr[ANGLE],
        FLT_TO_Q16(SAMPLE_TIME),
        -Q16_ONE, Q16_ONE, false, &pid_settings_arr[ANGLE]);
    pid_initialize(&my_config.pid_coeffs_arr[ANGLE_HIGH],
        FLT_TO_Q16(SAMPLE_TIME),
        -Q16_ONE, Q16_ONE, false, &pid_settings_arr[ANGLE_HIGH]);
    pid_initialize(&my_config.pid_coeffs_arr[VEL],
        FLT_TO_Q16(SAMPLE_TIME), FALL_LOWER_BOUND, FALL_UPPER_BOUND, true,
        &pid_settings_arr[VEL]);
    xSemaphoreGive(pid_mutex);

    mpu_set_gyro_offsets(my_config.gyro_offsets);
}

bool do_save_config(struct tcp_pcb *pcb) {
    uint8_t response;
    bool success = true;

    xSemaphoreTake(pid_mutex, portMAX_DELAY);
    success = sysparam_set_data("ANGLE_PID", (uint8_t *)&my_config.pid_coeffs_arr[ANGLE],
        sizeof(pid_coeffs), true) == SYSPARAM_OK;
    if (success) success = sysparam_set_data("ANGLE_HIGH_PID", (uint8_t *)&my_config.pid_coeffs_arr[ANGLE_HIGH], sizeof(pid_coeffs), true) == SYSPARAM_OK;
    if (success) success = sysparam_set_data("VEL_PID", (uint8_t *)&my_config.pid_coeffs_arr[VEL], sizeof(pid_coeffs), true) == SYSPARAM_OK;
    xSemaphoreGive(pid_mutex);
    if (success) success = sysparam_set_data("GYRO_OFFSETS", (uint8_t *)&my_config.gyro_offsets, 3 * sizeof(int16_t), true) == SYSPARAM_OK;

    if (success) {
        response = RES_SAVE_CONFIG_SUCCESS;
    } else {
        response = RES_SAVE_CONFIG_FAILURE;
    }
    websocket_write(pcb, &response, 1, WS_BIN_MODE);
    return success;
}

bool clear_flash_config() {
    uint32_t base_addr, num_sectors;
    return sysparam_get_info(&base_addr, &num_sectors) == SYSPARAM_OK &&
        sysparam_create_area(base_addr, num_sectors, true) == SYSPARAM_OK &&
        sysparam_init(base_addr, 0) == SYSPARAM_OK;
}

bool do_clear_config(struct tcp_pcb *pcb) {
    uint8_t response;
    // Clear the configuration by writing config version zero
    bool success = clear_flash_config();
    if (success) {
        response = RES_CLEAR_CONFIG_SUCCESS;
        load_hardcoded_config();
    } else {
        response = RES_CLEAR_CONFIG_FAILURE;
    }
    websocket_write(pcb, &response, 1, WS_BIN_MODE);
    return success;
}
