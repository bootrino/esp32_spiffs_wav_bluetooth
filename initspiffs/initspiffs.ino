#include "FS.h"
#include "SPIFFS.h"
#include "esp_partition.h"

#define FORMAT_SPIFFS_IF_FAILED true

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("- failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}


void listPartitions(void)
{
    size_t ul;
    esp_partition_iterator_t _mypartiterator;
    const esp_partition_t *_mypart;
    ul = spi_flash_get_chip_size();
    Serial.print("Flash chip size: ");
    Serial.println(ul);
    Serial.println("Partiton table:");
    _mypartiterator = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (_mypartiterator)
    {
        do
        {
            _mypart = esp_partition_get(_mypartiterator);
            printf("%x - %x - %x - %x - %s - %i\r\n", _mypart->type, _mypart->subtype, _mypart->address, _mypart->size, _mypart->label, _mypart->encrypted);
        } while ((_mypartiterator = esp_partition_next(_mypartiterator)));
    }
    esp_partition_iterator_release(_mypartiterator);
    _mypartiterator = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (_mypartiterator)
    {
        do
        {
            _mypart = esp_partition_get(_mypartiterator);
            printf("%x - %x - %x - %x - %s - %i\r\n", _mypart->type, _mypart->subtype, _mypart->address, _mypart->size, _mypart->label, _mypart->encrypted);
        } while ((_mypartiterator = esp_partition_next(_mypartiterator)));
    }
    esp_partition_iterator_release(_mypartiterator);
}



void setup()
{
    Serial.begin(115200);
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("SPIFFS mount failed, please wait up to a minute for formatting to complete");
        return;
    }
    listPartitions();
    listDir(SPIFFS, "/", 0);
    Serial.println("Finished");
}

void loop()
{
}