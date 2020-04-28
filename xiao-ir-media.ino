#include <IRremote.h>    // https://github.com/agn0stico/Arduino-IRremote-MKR1000/
#include <HID-Project.h> //https://github.com/NicoHood/HID
#include <FlashStorage.h>

const uint8_t TYPE_KEY = 0;
const uint8_t TYPE_SEQUENCE = 1;

typedef struct
{
    char desc[20];
    uint8_t type;
    uint8_t hidCodes[20];
    unsigned long irCode;
} KeyMapping;

// See HID key from https://github.com/NicoHood/HID/blob/master/src/KeyboardLayouts/ImprovedKeylayouts.h
const KeyMapping INIT_MAPPING[] = {
    {"UP", TYPE_KEY, KEY_UP},
    {"DOWN", TYPE_KEY, KEY_DOWN},
    {"LEFT", TYPE_KEY, KEY_LEFT},
    {"RIGHT", TYPE_KEY, KEY_RIGHT},
    {"ENTER", TYPE_KEY, KEY_ENTER},
    {"BACK-BACKSPACE", TYPE_KEY, KEY_BACKSPACE},
    {"HOME-ESC", TYPE_KEY, KEY_ESC},
    {"VOL_UP", TYPE_KEY, KEY_VOLUME_UP},
    {"VOL_DOWN", TYPE_KEY, KEY_VOLUME_DOWN},
    {"MUTE", TYPE_KEY, KEY_MUTE},
    {"PLAY/PAUSE-SPACE", TYPE_KEY, KEY_SPACE},
    {"Login", TYPE_SEQUENCE, "abcde"},
    {"Open/Close Kodi", TYPE_KEY, {KEY_LEFT_CTRL, KEY_LEFT_SHIFT, KEY_F12}},
    {"0", TYPE_KEY, KEYPAD_0},
    {"1", TYPE_KEY, KEYPAD_1},
    {"2", TYPE_KEY, KEYPAD_2},
    {"3", TYPE_KEY, KEYPAD_3},
    {"4", TYPE_KEY, KEYPAD_4},
    {"5", TYPE_KEY, KEYPAD_5},
    {"6", TYPE_KEY, KEYPAD_6},
    {"7", TYPE_KEY, KEYPAD_7},
    {"8", TYPE_KEY, KEYPAD_8},
    {"9", TYPE_KEY, KEYPAD_9},
};

const unsigned int KEY_LENGTH = sizeof(INIT_MAPPING) / sizeof(KeyMapping);

typedef struct
{
    KeyMapping kmap[KEY_LENGTH];
    decode_type_t irProtocolType = UNUSED;
    bool isValid = false;
} IrStoredMapping;

IrStoredMapping stored;
FlashStorage(ir_store_mapping, IrStoredMapping);

const int RECV_PIN = 10;
IRrecv irrecv(RECV_PIN);
decode_results results;
int lastMapIndex = -1;

void setup()
{
    Serial.begin(9600);

    irrecv.enableIRIn(); // Start the receiver
    Keyboard.begin();

    stored = ir_store_mapping.read();
    if (!stored.isValid)
    {
        while (!Serial)
            ; // Wait for serial to begin initialization
        initialize();
    }
    else
    {
        Serial.println("Configuration:");
        for (unsigned int it = 0; it < KEY_LENGTH; it++)
        {
            const KeyMapping &keycode = stored.kmap[it];
            Serial.print(keycode.desc);
            Serial.print(" => ");
            Serial.println(keycode.irCode, HEX);
        }
    }
    Serial.println();
    SerialUSB.setTimeout(0);
    irrecv.resume();
}

void initialize()
{
    Serial.println("Start initialization :");
    memcpy(stored.kmap, INIT_MAPPING, sizeof(INIT_MAPPING));
    stored.irProtocolType = UNUSED;
    unsigned int it = 0;
    while (it < KEY_LENGTH)
    {
        KeyMapping &keyMap = stored.kmap[it];

        Serial.print(keyMap.desc);
        Serial.print(" => ");
        Serial.flush();
        unsigned long start = millis();
        flushIr();
        irrecv.resume();
        while (1)
        {
            if (Serial.available() > 0)
            {
                char cmd = Serial.read();
                if (cmd == 'c')
                {
                    Serial.println("NONE");
                    it++;
                    break;
                }
                else if (cmd == 'p')
                {
                    Serial.println();
                    it = max(0, it - 1);
                    break;
                }
            }

            bool hasCode = irrecv.decode(&results);
            if (hasCode)
            {
                keyMap.irCode = results.value;
                irrecv.resume();
            }

            if (!hasCode || (results.decode_type == NEC && results.value == REPEAT))
            {
                delay(100);
                continue;
            }

            if (stored.irProtocolType == UNUSED)
            {
                stored.irProtocolType = results.decode_type;
            }

            if (stored.irProtocolType != results.decode_type)
            {
                Serial.println("Different type of protocol");
                break;
            }

            keyMap.irCode = results.value;
            Serial.println(keyMap.irCode, HEX);
            delay(500);
            it++;
            break;
        }
    }
    Serial.print("Protocol ");
    Serial.println(stored.irProtocolType);
    stored.isValid = true;
    ir_store_mapping.write(stored);
    flushIr();
    lastMapIndex = -1;
    Serial.println("Configuration finished");
}

void flushIr()
{
    irrecv.decode(&results);
}

int getMappingFromIr(unsigned long irValue)
{
    int it = 0;
    while (it < KEY_LENGTH)
    {
        const KeyMapping &keyMap = stored.kmap[it];
        if (keyMap.irCode == irValue)
        {
            break;
        }
        it++;
    }

    if (it >= KEY_LENGTH)
    {
        it = -1;
    }

    return it;
}

void keyWrite(const KeyMapping &keyMap)
{
    if (keyMap.type == TYPE_KEY)
    {
        const uint8_t *keyboardCode = keyMap.hidCodes;
        const uint8_t len = strlen((const char *)keyboardCode);
        for (uint8_t it = 0; it < len; it++)
        {
            Keyboard.press(KeyboardKeycode(keyboardCode[it]));
        }
        Keyboard.releaseAll();
    }
    else if (keyMap.type == TYPE_SEQUENCE)
    {
        Keyboard.print((char *)keyMap.hidCodes);
    }
}

void loop()
{
    if (SerialUSB.readStringUntil('\n') == "configure")
    {
        initialize();
    }
    bool hasCode = irrecv.decode(&results);
    if (hasCode && irrecv.decode(&results) && (stored.irProtocolType == results.decode_type))
    {

        if (stored.irProtocolType != NEC || results.value != REPEAT)
        {
            lastMapIndex = getMappingFromIr(results.value);
        }
        Serial.print("IR value "); Serial.print(results.value, HEX); Serial.print(" => ");
        if (lastMapIndex < 0)
        {
            Serial.println("NOT FOUND");
        }
        else
        {
            const KeyMapping &keyMap = stored.kmap[lastMapIndex];
            Serial.println(keyMap.desc);
            keyWrite(keyMap);
        }
    }

    if (hasCode)
    {
        irrecv.resume();
    }

    delay(100);
}
