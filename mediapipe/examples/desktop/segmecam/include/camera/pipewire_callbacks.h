#ifndef SEGMECAM_PIPEWIRE_CALLBACKS_H_
#define SEGMECAM_PIPEWIRE_CALLBACKS_H_

#include <pipewire/pipewire.h>

namespace segmecam {

// Forward declarations
struct PipeWireEvents;

// Static callback functions for PipeWire stream events
void on_stream_state_changed(void* data, enum pw_stream_state old_state,
                            enum pw_stream_state new_state, const char* error);
void on_stream_format_changed(void* data, uint32_t id, const struct spa_pod* param);
void on_stream_process(void* data);

} // namespace segmecam

#endif // SEGMECAM_PIPEWIRE_CALLBACKS_H_