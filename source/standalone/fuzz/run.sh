#!/bin/sh
# libFuzzer runs of every parser that reads outside data. SECONDS per target (default 60).
set -e
cd "$(dirname "$0")"; F=../firmware; O=${TMPDIR:-/tmp}/wasel-fuzz; T=${SECONDS_PER_TARGET:-60}; mkdir -p $O
CC="clang -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -DQABES_REMOTE=1 -I$F -w"
$CC fuzz_settings.c $F/settings.c $F/settings_record.c $F/protection.c $F/scheduler.c -o $O/settings
$CC fuzz_qabes.c $F/qabes_remote.c $F/qabes_aead.c -o $O/qabes
$CC fuzz_wifi_form.c $F/wifi_form.c -o $O/wifi_form
$CC fuzz_mqtt.c $F/mqtt_wire.c $F/mqtt_command.c $F/mqtt_session.c $F/settings.c $F/protection.c $F/scheduler.c $F/core.c -o $O/mqtt
mkdir -p $O/corpus-settings $O/corpus-qabes
cc -w -I$F seeds.c $F/settings.c $F/protection.c $F/scheduler.c $F/qabes_remote.c $F/qabes_aead.c -o $O/seeds && $O/seeds $O
for t in settings qabes wifi_form mqtt; do
  mkdir -p $O/corpus-$t; echo "== $t"
  # A crash, hang or sanitizer finding fails the run and prints the input that caused it.
  if ! $O/$t -max_total_time=$T -max_len=2048 -timeout=5 -print_final_stats=1 -artifact_prefix=$O/$t- $O/corpus-$t >$O/$t.log 2>&1; then tail -60 $O/$t.log; exit 1; fi
  grep -E "^Done|number_of_executed_units" $O/$t.log
done
