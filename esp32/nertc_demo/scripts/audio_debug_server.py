import socket
import wave
import argparse


'''
  Create a UDP socket and bind it to the server's IP:8000.
  Listen for incoming messages and print them to the console.
  Save the audio to a WAV file.
'''
def main(samplerate, channels, port, input_codec, frame_duration_ms, output):
    decoder = None
    frame_size = None
    if input_codec == "opus":
        try:
            import opuslib
        except ImportError as exc:
            raise SystemExit(
                "Opus mode requires opuslib. Install it with: pip install opuslib"
            ) from exc
        decoder = opuslib.Decoder(samplerate, channels)
        frame_size = int(samplerate * frame_duration_ms / 1000)
        if frame_size <= 0:
            raise SystemExit("Invalid --frame-duration-ms, frame size must be > 0")

    # Create a UDP socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_socket.bind(('0.0.0.0', port))

    # Create WAV file with parameters
    filename = output if output else f"{input_codec}_{samplerate}_{channels}.wav"
    wav_file = wave.open(filename, "wb")
    wav_file.setnchannels(channels)     # channels parameter
    wav_file.setsampwidth(2)            # 2 bytes per sample (16-bit)
    wav_file.setframerate(samplerate)   # samplerate parameter

    print(f"Start saving {input_codec} audio from 0.0.0.0:{port} to {filename}...")

    try:
        while True:
            # Receive a message from the client
            message, address = server_socket.recvfrom(8000)

            if input_codec == "opus":
                try:
                    pcm_data = decoder.decode(message, frame_size)
                except Exception as exc:
                    print(f"Decode failed from {address}, packet={len(message)} bytes, error={exc}")
                    continue
                wav_file.writeframes(pcm_data)
            else:
                # Write PCM data to WAV file
                wav_file.writeframes(message)

            # Print length of the message
            print(f"Received {len(message)} bytes from {address}")
    
    except KeyboardInterrupt:
        print("\nStopping recording...")
    
    finally:
        # Close files and socket
        wav_file.close()
        server_socket.close()
        print(f"WAV file '{filename}' saved successfully")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='UDP音频数据接收器，保存为WAV文件')
    parser.add_argument('--input-codec', choices=['pcm16', 'opus'], default='pcm16',
                        help='输入数据格式: pcm16 或 opus (默认: pcm16)')
    parser.add_argument('--samplerate', '-s', type=int, default=16000, 
                        help='采样率 (默认: 16000)')
    parser.add_argument('--channels', '-c', type=int, default=None,
                        help='声道数 (默认: pcm16=2, opus=1)')
    parser.add_argument('--frame-duration-ms', type=int, default=20,
                        help='Opus每包时长ms，仅在 --input-codec opus 时使用 (默认: 20)')
    parser.add_argument('--port', '-p', type=int, default=8000,
                        help='UDP监听端口 (默认: 8000)')
    parser.add_argument('-o', '--output', type=str, default=None,
                        help='输出WAV文件名 (默认: 自动命名)')
    
    args = parser.parse_args()
    channels = args.channels
    if channels is None:
        channels = 1 if args.input_codec == 'opus' else 2
    main(args.samplerate, channels, args.port, args.input_codec, args.frame_duration_ms, args.output)
