import os
import csv
from functools import wraps
from flask import Flask, request, jsonify, render_template, Response
from datetime import datetime
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

app = Flask(__name__)

# Configuration - use script directory for CSV
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(SCRIPT_DIR, "sensor_data.csv")
PORT = int(os.getenv("PORT", 5001))

# Authentication credentials
AUTH_USERNAME = os.getenv("AUTH_USERNAME", "admin")
AUTH_PASSWORD = os.getenv("AUTH_PASSWORD", "admin")


def check_auth(username, password):
    """Check if username/password combination is valid."""
    return username == AUTH_USERNAME and password == AUTH_PASSWORD


def authenticate():
    """Send 401 response to enable basic auth."""
    return Response(
        "Acesso negado. Por favor, forneça credenciais válidas.",
        401,
        {"WWW-Authenticate": 'Basic realm="BitDogLab Dashboard"'},
    )


def requires_auth(f):
    """Decorator for routes that require authentication."""

    @wraps(f)
    def decorated(*args, **kwargs):
        auth = request.authorization
        if not auth or not check_auth(auth.username, auth.password):
            return authenticate()
        return f(*args, **kwargs)

    return decorated


# Initialize CSV if it doesn't exist
if not os.path.exists(DATA_FILE):
    with open(DATA_FILE, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "timestamp",
                "lux",
                "temp",
                "rssi",
                "uptime",
                "accel_x",
                "accel_y",
                "accel_z",
            ]
        )


@app.route("/")
@requires_auth
def index():
    return render_template("index.html")


@app.route("/submit_data", methods=["POST"])
def submit_data():
    try:
        data = request.json
        if not data:
            return jsonify({"error": "No data provided"}), 400

        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        lux = data.get("lux", 0)
        temp = data.get("temp", 0)
        rssi = data.get("rssi", 0)
        uptime = data.get("uptime", 0)
        accel = data.get("accel", {})
        ax = accel.get("x", 0)
        ay = accel.get("y", 0)
        az = accel.get("z", 0)

        # Save to CSV
        with open(DATA_FILE, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([timestamp, lux, temp, rssi, uptime, ax, ay, az])

        print(
            f"[{timestamp}] Lux={lux:.1f} Temp={temp:.1f}°C RSSI={rssi}dBm Uptime={uptime}s Accel=({ax},{ay},{az})"
        )
        return jsonify({"status": "success", "message": "Data saved"}), 200

    except Exception as e:
        print(f"Error saving data: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/api/data", methods=["GET"])
@requires_auth
def get_data():
    results = []
    try:
        with open(DATA_FILE, "r") as f:
            reader = csv.DictReader(f)
            data = []
            for row in reader:
                # Filter out rows with None values
                if all(v is not None and v != "" for v in row.values()):
                    data.append(row)
            results = data[-50:]
    except Exception as e:
        print(f"Error reading data: {e}")
    return jsonify(results)


if __name__ == "__main__":
    print(f"Starting server on port {PORT}...")
    app.run(host="0.0.0.0", port=PORT, debug=True)
