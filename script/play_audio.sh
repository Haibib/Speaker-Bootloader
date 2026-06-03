file="${1:-"test.wav"}"
output_file="${file%.wav}.h"
AUDIO_DIR="./audio"
TEST_DIR="./speaker-test"

xxd -i "$AUDIO_DIR/$file" > "$TEST_DIR/$output_file"

