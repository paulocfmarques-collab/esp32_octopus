#pragma once
#include <Arduino.h>
#include "StorageManager.h"

namespace CrashDiagnostics
{
String motivoReset();
String lerArquivarEApagar(StorageManager& storage);
} // namespace CrashDiagnostics
