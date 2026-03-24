import re
import base64
import json
import logging
import os
import requests

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Shared prompt for all providers
OCR_PROMPT = """Analyze this image and perform the following tasks:
1. Extract ALL visible text from the image (OCR).
2. Detect the language of the extracted text.
3. Translate the extracted text professionally:
   - If the original text is in Arabic, translate it to English.
   - If the original text is in English or any other language, translate it to Arabic.

Respond ONLY with valid JSON in this exact format, no markdown or extra text:
{
  "extracted_text": "the original text found in the image",
  "source_language": "detected language name",
  "translated_text": "translation of the text",
  "confidence": 0.95
}

If no text is found in the image, respond with:
{
  "extracted_text": "",
  "source_language": "unknown",
  "translated_text": "",
  "confidence": 0.0
}"""


class BaseProvider:
    """Base class for AI providers"""

    def __init__(self, api_key, name):
        self.api_key = api_key
        self.name = name

    def extract_and_translate(self, image_bytes):
        raise NotImplementedError

    def verify_connection(self) -> tuple[bool, str]:
        """Verify API key and connection properties before use. Returns (success, message)."""
        raise NotImplementedError

    def _encode_image(self, image_bytes):
        return base64.b64encode(image_bytes).decode('utf-8')

    def _parse_response(self, text):
        """Parse JSON from AI response robustly"""
        text = text.strip()
        
        # Use regex to find the JSON object, handles markdown formatting like ```json ... ```
        match = re.search(r'\{[\s\S]*\}', text)
        if match:
            text = match.group(0)

        try:
            data = json.loads(text)
            return {
                'success': True,
                'text': data.get('extracted_text', ''),
                'source_language': data.get('source_language', 'unknown'),
                'translated_text': data.get('translated_text', ''),
                'confidence': float(data.get('confidence', 0.0)),
                'provider': self.name,
                'error': None
            }
        except json.JSONDecodeError as e:
            logger.error(f'Failed to parse AI response: {e}')
            logger.error(f'Raw response: {text[:500]}')
            # Try to extract text even if JSON parsing fails
            return {
                'success': True,
                'text': text,
                'source_language': 'unknown',
                'translated_text': '',
                'confidence': 0.0,
                'provider': self.name,
                'error': None
            }


class GeminiProvider(BaseProvider):
    """Google Gemini API provider"""

    def __init__(self, api_key):
        super().__init__(api_key, 'gemini')
        self.base_url = 'https://generativelanguage.googleapis.com/v1beta'
        self.model = 'gemini-2.0-flash'

    def extract_and_translate(self, image_bytes):
        try:
            b64_image = self._encode_image(image_bytes)
            url = f'{self.base_url}/models/{self.model}:generateContent?key={self.api_key}'

            payload = {
                'contents': [{
                    'parts': [
                        {'text': OCR_PROMPT},
                        {
                            'inlineData': {
                                'mimeType': 'image/jpeg',
                                'data': b64_image
                            }
                        }
                    ]
                }],
                'generationConfig': {
                    'temperature': 0.1,
                    'maxOutputTokens': 2048
                }
            }

            response = requests.post(url, json=payload, timeout=30)
            response.raise_for_status()

            data = response.json()
            text = data['candidates'][0]['content']['parts'][0]['text']
            logger.info(f'[Gemini] Response received')
            return self._parse_response(text)

        except requests.exceptions.Timeout:
            return self._error('Gemini API timeout')
        except requests.exceptions.RequestException as e:
            return self._error(f'Gemini API error: {str(e)}')
        except (KeyError, IndexError) as e:
            return self._error(f'Gemini response parsing error: {str(e)}')

    def _error(self, msg):
        logger.error(msg)
        return {'success': False, 'text': '', 'translated_text': '',
                'confidence': 0.0, 'provider': self.name, 'error': msg}

    def verify_connection(self):
        try:
            url = f'{self.base_url}/models?key={self.api_key}'
            resp = requests.get(url, timeout=10)
            if resp.status_code == 200:
                return True, "Gemini API Key is valid"
            return False, f"Invalid API Key (HTTP {resp.status_code})"
        except Exception as e:
            return False, f"Connection error: {str(e)}"


class OpenAIProvider(BaseProvider):
    """OpenAI GPT-4o API provider"""

    def __init__(self, api_key):
        super().__init__(api_key, 'openai')
        self.base_url = 'https://api.openai.com/v1'
        self.model = 'gpt-4o'

    def extract_and_translate(self, image_bytes):
        try:
            b64_image = self._encode_image(image_bytes)
            url = f'{self.base_url}/chat/completions'

            headers = {
                'Authorization': f'Bearer {self.api_key}',
                'Content-Type': 'application/json'
            }

            payload = {
                'model': self.model,
                'messages': [{
                    'role': 'user',
                    'content': [
                        {'type': 'text', 'text': OCR_PROMPT},
                        {
                            'type': 'image_url',
                            'image_url': {
                                'url': f'data:image/jpeg;base64,{b64_image}',
                                'detail': 'high'
                            }
                        }
                    ]
                }],
                'max_tokens': 2048,
                'temperature': 0.1
            }

            response = requests.post(url, headers=headers, json=payload, timeout=30)
            response.raise_for_status()

            data = response.json()
            text = data['choices'][0]['message']['content']
            logger.info(f'[OpenAI] Response received')
            return self._parse_response(text)

        except requests.exceptions.Timeout:
            return self._error('OpenAI API timeout')
        except requests.exceptions.RequestException as e:
            return self._error(f'OpenAI API error: {str(e)}')
        except (KeyError, IndexError) as e:
            return self._error(f'OpenAI response parsing error: {str(e)}')

    def _error(self, msg):
        logger.error(msg)
        return {'success': False, 'text': '', 'translated_text': '',
                'confidence': 0.0, 'provider': self.name, 'error': msg}

    def verify_connection(self):
        try:
            url = f'{self.base_url}/models'
            headers = {'Authorization': f'Bearer {self.api_key}'}
            resp = requests.get(url, headers=headers, timeout=10)
            if resp.status_code == 200:
                return True, "OpenAI API Key is valid"
            return False, f"Invalid API Key (HTTP {resp.status_code})"
        except Exception as e:
            return False, f"Connection error: {str(e)}"


class AnthropicProvider(BaseProvider):
    """Anthropic Claude API Provider"""

    def __init__(self, api_key):
        super().__init__(api_key, 'anthropic')
        self.base_url = 'https://api.anthropic.com/v1'
        self.model = 'claude-3-haiku-20240307'  # Fast and cheap Claude 3 model supporting Vision

    def extract_and_translate(self, image_bytes):
        try:
            b64_image = self._encode_image(image_bytes)
            url = f'{self.base_url}/messages'

            headers = {
                'x-api-key': self.api_key,
                'anthropic-version': '2023-06-01',
                'content-type': 'application/json'
            }

            payload = {
                'model': self.model,
                'max_tokens': 2048,
                'temperature': 0.1,
                'messages': [{
                    'role': 'user',
                    'content': [
                        {
                            'type': 'image',
                            'source': {
                                'type': 'base64',
                                'media_type': 'image/jpeg',
                                'data': b64_image
                            }
                        },
                        {
                            'type': 'text',
                            'text': OCR_PROMPT
                        }
                    ]
                }]
            }

            response = requests.post(url, headers=headers, json=payload, timeout=12) # 12s timeout for ESP32
            response.raise_for_status()

            data = response.json()
            text = data['content'][0]['text']
            logger.info(f'[Anthropic API] Response received')
            return self._parse_response(text)

        except requests.exceptions.Timeout:
            return self._error('Anthropic API timeout. Try again.')
        except requests.exceptions.RequestException as e:
            return self._error(f'Anthropic API error: {str(e)}')
        except (KeyError, IndexError) as e:
            return self._error(f'Anthropic API parsing error: {str(e)}')

    def _error(self, msg):
        logger.error(msg)
        return {'success': False, 'text': '', 'translated_text': '',
                'confidence': 0.0, 'provider': self.name, 'error': msg}

    def verify_connection(self):
        try:
            url = f'{self.base_url}/messages'
            headers = {
                'x-api-key': self.api_key,
                'anthropic-version': '2023-06-01',
                'content-type': 'application/json'
            }
            # Simple text payload to verify key works
            payload = {
                "model": "claude-3-haiku-20240307",
                "max_tokens": 10,
                "messages": [{"role": "user", "content": "Hi"}]
            }
            resp = requests.post(url, headers=headers, json=payload, timeout=10)
            if resp.status_code == 200:
                return True, "Anthropic API Key is valid!"
            return False, f"Invalid API Key (HTTP {resp.status_code})"
        except Exception as e:
            return False, f"Connection error: {str(e)}"


def get_available_providers():
    """Return list of providers with valid API keys"""
    providers = []

    if os.environ.get('GEMINI_API_KEY'):
        providers.append({'id': 'gemini', 'name': 'Google Gemini'})
    if os.environ.get('OPENAI_API_KEY'):
        providers.append({'id': 'openai', 'name': 'OpenAI GPT-4o'})
    if os.environ.get('ANTHROPIC_API_KEY'):
        providers.append({'id': 'anthropic', 'name': 'Anthropic (Claude 3 Haiku)'})

    return providers


def get_provider(provider_id=None):
    """Get a provider instance by ID. Falls back to first available."""
    if not provider_id:
        provider_id = os.environ.get('DEFAULT_PROVIDER', 'gemini')

    if provider_id == 'gemini':
        key = os.environ.get('GEMINI_API_KEY')
        if key:
            return GeminiProvider(key)

    elif provider_id == 'openai':
        key = os.environ.get('OPENAI_API_KEY')
        if key:
            return OpenAIProvider(key)

    elif provider_id == 'anthropic':
        key = os.environ.get('ANTHROPIC_API_KEY')
        if key:
            return AnthropicProvider(key)

    # Fallback: try any available provider
    for pid in ['anthropic', 'gemini', 'openai']:
        if pid != provider_id:
            try:
                return get_provider(pid)
            except Exception:
                continue

    raise ValueError('No AI provider configured. Set at least one API key in .env')
