import urllib.request
import time
import os

print(f"PID: {os.getpid()}")

URL = "https://www.google.com"
REQUESTS = 1000

print(f"Enviando {REQUESTS} requisições para {URL}...")

for i in range(REQUESTS):
    try:
        with urllib.request.urlopen(URL) as response:
            response.read()
        print(f"Requisição {i+1}/{REQUESTS} enviada.", end='\r')
    except Exception as e:
        print(f"\nErro na requisição {i+1}: {e}")

print("\nConcluído.")

