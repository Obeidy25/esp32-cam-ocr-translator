from flask import Flask, request, jsonify
import logging
import os
import requests as http_requests
from dotenv import load_dotenv
from ai_provider import get_provider, get_available_providers

# Load environment variables, force override if terminal cached the old vars
load_dotenv(override=True)

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 10 * 1024 * 1024  # 10 MB

@app.after_request
def add_cors_headers(response):
    response.headers['Access-Control-Allow-Origin'] = '*'
    response.headers['Access-Control-Allow-Headers'] = 'Content-Type,Authorization'
    response.headers['Access-Control-Allow-Methods'] = 'GET,PUT,POST,DELETE,OPTIONS'
    # Prevent aggressive browser caching from keeping the dropdown empty
    response.headers['Cache-Control'] = 'no-cache, no-store, must-revalidate'
    response.headers['Expires'] = '0'
    response.headers['Pragma'] = 'no-cache'
    return response

@app.route('/health', methods=['GET'])
def health():
    return jsonify({'status': 'ok'}), 200

@app.route('/verify', methods=['GET', 'OPTIONS'])
def verify_provider():
    if request.method == 'OPTIONS':
        return '', 204
    provider_id = request.args.get('provider')
    if not provider_id:
        return jsonify({'success': False, 'message': 'Provider required'}), 400
    try:
        provider = get_provider(provider_id)
        success, message = provider.verify_connection()
        return jsonify({'success': success, 'message': message}), 200
    except Exception as e:
        return jsonify({'success': False, 'message': str(e)}), 400


@app.route('/providers', methods=['GET'])
def providers():
    """Return available AI providers based on configured API keys"""
    available = get_available_providers()
    default = os.environ.get('DEFAULT_PROVIDER', 'gemini')
    return jsonify({'providers': available, 'default': default}), 200


@app.route('/process-image', methods=['POST'])
def process_image():
    """Process image: OCR + translate using selected AI provider"""
    try:
        # Get image bytes
        if 'image' in request.files:
            image_bytes = request.files['image'].read()
        elif request.data:
            image_bytes = request.data
        else:
            return jsonify({'success': False, 'error': 'No image provided'}), 400

        if len(image_bytes) == 0:
            return jsonify({'success': False, 'error': 'Image is empty'}), 400

        # Get selected provider
        provider_id = request.args.get('provider') or request.form.get('provider')

        logger.info(f'Processing image ({len(image_bytes)} bytes) with provider: {provider_id or "default"}')

        # Get AI provider and process
        provider = get_provider(provider_id)
        result = provider.extract_and_translate(image_bytes)

        # Send to Telegram if configured
        if result['success']:
            send_to_telegram(result, image_bytes)

        status_code = 200 if result['success'] else 400
        return jsonify(result), status_code

    except ValueError as e:
        return jsonify({'success': False, 'error': str(e)}), 400
    except Exception as e:
        logger.error(f'Unexpected error: {str(e)}', exc_info=True)
        return jsonify({'success': False, 'error': f'Server error: {str(e)}'}), 500


def send_to_telegram(result, image_bytes):
    """Send translation results to Telegram bot (non-blocking, won't crash on failure)"""
    bot_token = os.environ.get('TELEGRAM_BOT_TOKEN', '')
    chat_id = os.environ.get('TELEGRAM_CHAT_ID', '')
    if not bot_token or not chat_id:
        return
    try:
        text = f"📸 *OCR Translation*\n\n"
        text += f"🔤 *Original ({result.get('source_language', '?')}):*\n{result.get('text', '')}\n\n"
        text += f"🌍 *Translation:*\n{result.get('translated_text', '')}\n\n"
        text += f"🤖 Provider: {result.get('provider', 'unknown')}"
        
        # Send image with caption
        url = f"https://api.telegram.org/bot{bot_token}/sendPhoto"
        http_requests.post(url, data={'chat_id': chat_id, 'caption': text, 'parse_mode': 'Markdown'},
                          files={'photo': ('capture.jpg', image_bytes, 'image/jpeg')}, timeout=10)
        logger.info('[Telegram] Sent successfully')
    except Exception as e:
        logger.warning(f'[Telegram] Failed: {e}')


@app.route('/send-telegram', methods=['POST', 'OPTIONS'])
def manual_send_telegram():
    """Send text to Telegram on-demand from the UI"""
    if request.method == 'OPTIONS':
        return '', 204
    bot_token = os.environ.get('TELEGRAM_BOT_TOKEN', '')
    chat_id = os.environ.get('TELEGRAM_CHAT_ID', '')
    if not bot_token or not chat_id:
        return jsonify({'success': False, 'error': 'Telegram not configured. Set TELEGRAM_BOT_TOKEN and TELEGRAM_CHAT_ID in .env'}), 400
    try:
        data = request.get_json(force=True)
        text = data.get('text', '')
        label = data.get('label', 'Text')
        if not text:
            return jsonify({'success': False, 'error': 'No text provided'}), 400

        msg = f"📝 *{label}*\n\n{text}"
        url = f"https://api.telegram.org/bot{bot_token}/sendMessage"
        resp = http_requests.post(url, json={'chat_id': chat_id, 'text': msg, 'parse_mode': 'Markdown'}, timeout=10)
        if resp.status_code == 200:
            logger.info('[Telegram] Manual send successful')
            return jsonify({'success': True}), 200
        else:
            try:
                error_data = resp.json()
                error_msg = error_data.get('description', f'API error: {resp.status_code}')
            except:
                error_msg = f'API error: {resp.status_code}'
            return jsonify({'success': False, 'error': error_msg}), 400
    except Exception as e:
        logger.warning(f'[Telegram] Manual send failed: {e}')
        return jsonify({'success': False, 'error': str(e)}), 500


@app.errorhandler(413)
def too_large(error):
    return jsonify({'success': False, 'error': 'Image too large (max 10MB)'}), 413


if __name__ == '__main__':
    port = int(os.environ.get('PORT', 5000))
    available = get_available_providers()
    logger.info(f'Server starting on port {port}')
    logger.info(f'Available providers: {[p["name"] for p in available]}')
    if not available:
        logger.warning('No AI providers configured! Set API keys in .env')
    app.run(host='0.0.0.0', port=port, debug=True)
