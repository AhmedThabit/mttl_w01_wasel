#!/bin/sh
# Host tests of the Qabes remote channel (RFC 8439 vectors, frame rules, engine flow).
set -e
cd "$(dirname "$0")"; F=../firmware; O=${TMPDIR:-/tmp}
cc -std=gnu99 -O2 -I$F test_qabes_aead.c $F/qabes_aead.c -o $O/t_aead && $O/t_aead
cc -std=gnu99 -O2 -I$F test_qabes_remote.c $F/qabes_remote.c $F/qabes_aead.c -o $O/t_remote && $O/t_remote
cc -std=gnu99 -O1 -DQABES_REMOTE=1 -I$F test_qabes_engine.c $F/device_app.c $F/mqtt_engine.c $F/mqtt_session.c $F/mqtt_wire.c $F/mqtt_command.c $F/ha_discovery.c $F/core.c $F/settings.c $F/protection.c $F/scheduler.c $F/ntp.c $F/ntp_client.c $F/hardware_port.c $F/config_store.c $F/settings_store.c $F/settings_record.c $F/qabes_remote.c $F/qabes_aead.c -o $O/t_engine && $O/t_engine
cc -std=gnu99 -O1 -DQABES_REMOTE=1 -I$F test_cloud_login.c $F/device_app.c $F/mqtt_engine.c $F/mqtt_session.c $F/mqtt_wire.c $F/mqtt_command.c $F/ha_discovery.c $F/core.c $F/settings.c $F/protection.c $F/scheduler.c $F/ntp.c $F/ntp_client.c $F/hardware_port.c $F/config_store.c $F/settings_store.c $F/settings_record.c $F/qabes_remote.c $F/qabes_aead.c -o $O/t_cloud && $O/t_cloud
sed -e 's/qabes_chacha20/ref_chacha20/g; s/qabes_poly1305/ref_poly1305/g; s/qabes_aead_seal/ref_seal/g; s/qabes_aead_open/ref_open/g; s/qabes_aead_ready/ref_ready/g; s/#include "qabes_aead.h"/#include <stdint.h>\n#include <stddef.h>/' $F/qabes_aead.c > $O/ref_aead.c
for f in 1 2; do cc -std=gnu99 -w -DFORMSEL=$f -I$F test_qabes_rom.c $F/qabes_aead_rom.c $O/ref_aead.c -o $O/t_rom && $O/t_rom; done
cc -std=gnu99 -D_GNU_SOURCE -O1 -w -I$F test_wasel_settings.c $F/settings.c $F/settings_record.c $F/protection.c $F/scheduler.c $F/qabes_remote.c $F/qabes_aead.c -o $O/t_settings && $O/t_settings
cc -std=gnu99 -O1 -I$F test_find.c $F/hardware_port.c $F/core.c -o $O/t_find && $O/t_find
