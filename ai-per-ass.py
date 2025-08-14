from flask import Flask, send_file

app = Flask(__name__)

@app.route('/audio.wav')
def serve_wav():
    return send_file('audio.wav', mimetype='audio/wav')

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8000)