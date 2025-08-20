from flask import Flask, request, send_file
import os
import time
from gtts import gTTS
from pydub import AudioSegment
import speech_recognition as sr
from openai import OpenAI

# CONFIG
OPENAI_API_KEY = '' #use your api key
client = OpenAI(api_key=OPENAI_API_KEY)

app = Flask(__name__)

# Folder to store uploaded and generated files
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
UPLOAD_FOLDER = os.path.join(BASE_DIR, "uploads")
os.makedirs(UPLOAD_FOLDER, exist_ok=True)


@app.route("/upload", methods=["POST"])
def upload_audio():
    """Receive audio from client and save it"""
    mic_path = os.path.join(UPLOAD_FOLDER, "microphone.wav")
    response_wav = os.path.join(UPLOAD_FOLDER, "response.wav")
    temp_mp3 = os.path.join(UPLOAD_FOLDER, "temp.mp3")

    # Remove old files if they exist
    for f in [mic_path, response_wav, temp_mp3]:
        if os.path.exists(f):
            os.remove(f)
            print(f"[CLEANUP] Removed old file: {f}")

    # Save uploaded audio
    with open(mic_path, "wb") as f:
        f.write(request.data)
    print(f"[UPLOAD] File saved: {mic_path}")

    # Process audio: STT → GPT → TTS → WAV
    text = audio_to_text("microphone.wav")
    if not text:
        text = "I could not understand the audio."

    text_to_wav(text, response_wav)

    return "OK", 200


@app.route("/response.wav")
def serve_wav():
    """Serve the generated WAV file, wait briefly if it is not ready"""
    wav_path = os.path.join(UPLOAD_FOLDER, "response.wav")
    timeout = 5  # maximum 5 seconds
    while timeout > 0 and not os.path.exists(wav_path):
        time.sleep(0.1)
        timeout -= 0.1

    if not os.path.exists(wav_path):
        return "File not found", 404
    return send_file(wav_path, mimetype="audio/wav")


def audio_to_text(wav_filename):
    """Convert WAV file to text using Google STT"""
    file_path = os.path.join(UPLOAD_FOLDER, wav_filename)

    if not os.path.exists(file_path):
        print("[ERROR] Audio file not found:", file_path)
        return None

    recognizer = sr.Recognizer()
    with sr.AudioFile(file_path) as source:
        audio_data = recognizer.record(source)
        try:
            text = recognizer.recognize_google(audio_data, language="en-US")
            print("[STT] Recognized text:", text)
            return chatgpt_response(text)
        except sr.UnknownValueError:
            print("[STT] Could not understand audio")
        except sr.RequestError as e:
            print("[STT] Request error:", e)
    return None


def text_to_wav(text, wav_path):
    """Generate WAV file from text using gTTS"""
    mp3_path = os.path.join(UPLOAD_FOLDER, "temp.mp3")

    tts = gTTS(text, lang="en")
    tts.save(mp3_path)

    audio = AudioSegment.from_mp3(mp3_path)
    audio = audio.set_frame_rate(44100).set_channels(2).set_sample_width(2)
    audio.export(wav_path, format="wav")

    print(f"[TTS] WAV file saved at {wav_path}")
    #time.sleep(0.1)
    return wav_path


def chatgpt_response(prompt):
    """Send prompt to OpenAI GPT and get response"""
    try:
        response = client.chat.completions.create(
            model="gpt-4o-mini",
            messages=[
                {"role": "system", "content": "Provide max 10 tokens per answer"},
                {"role": "user", "content": prompt},
            ],
            max_tokens=999,
            temperature=0.7,
        )

        content = response.choices[0].message.content.strip()
        print("[GPT] Response ready:", content)
        return content

    except Exception as e:
        print("[GPT] API error:", e)
        return f"API error: {e}"


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
