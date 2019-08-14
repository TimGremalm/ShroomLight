#define BLINK_GPIO 16

enum BLINKSTATE {
	BLINKSTATE_Idle,
	BLINKSTATE_Detect,
	BLINKSTATE_OTA,
	BLINKSTATE_OTA_FAILED
};

typedef struct {
	enum BLINKSTATE state;
} statsblink_config_t;

void statusblinktask(void *pvParameters);
void setblinkstate(enum BLINKSTATE state);

