#pragma once
// Line-delimited JSON over USB serial, used by the web Settings page.
namespace proto {
void poll();  // call every loop; non-blocking
}
