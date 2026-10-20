"""
xyz Search Proxy Server
Plain HTTP endpoint - ESP32 friendly, no SSL needed on ESP32 side.
Render handles HTTPS externally, but we add a plain HTTP option too.
"""

from flask import Flask, request, jsonify, Response
import requests
import re
import json

app = Flask(__name__)

def clean(text, maxlen=88):
    if not text: return ""
    text = re.sub(r'\s+', ' ', text).strip()
    text = re.sub(r'\([^)]{0,40}\)', '', text).strip()
    text = re.sub(r'\s+', ' ', text).strip()
    if len(text) > maxlen:
        text = text[:maxlen-2] + ".."
    return text

def short(text, maxlen=26):
    text = text.strip()
    if len(text) > maxlen:
        return text[:maxlen-2] + ".."
    return text

def wiki_search(query):
    results = []
    try:
        r = requests.get(
            "https://en.wikipedia.org/w/api.php",
            params={"action":"opensearch","search":query,"limit":5,"namespace":0,"format":"json"},
            timeout=8, headers={"User-Agent":"xyz/1.0"}
        )
        data = r.json()
        titles = data[1] if len(data)>1 else []
        raw_urls = data[3] if len(data)>3 else []
        if not titles: return []
        for i, title in enumerate(titles[:5]):
            try:
                r2 = requests.get(
                    "https://en.wikipedia.org/w/api.php",
                    params={"action":"query","titles":title,"prop":"extracts","exintro":1,"explaintext":1,"exsentences":2,"format":"json","formatversion":2},
                    timeout=6, headers={"User-Agent":"xyz/1.0"}
                )
                pages = r2.json().get("query",{}).get("pages",[])
                extract = pages[0].get("extract","") if pages else ""
            except: extract=""
            url = raw_urls[i] if i<len(raw_urls) else ""
            url = url.replace("https://en.wikipedia.org/wiki/","").replace("_"," ")
            results.append({"t":short(title),"s":clean(extract) or "See Wikipedia","u":short(url,28)})
        return results
    except Exception as e:
        print(f"Wiki error: {e}")
        return []

def ddg_search(query):
    try:
        r = requests.get("https://api.duckduckgo.com/",
            params={"q":query,"format":"json","no_html":1,"skip_disambig":1},
            timeout=8, headers={"User-Agent":"xyz/1.0"})
        data = r.json()
        text = data.get("AbstractText","") or data.get("Answer","") or data.get("Definition","")
        if text:
            return [{"t":short(query),"s":clean(text),"u":"duckduckgo.com"}]
        results=[]
        for item in data.get("RelatedTopics",[])[:4]:
            if isinstance(item,dict) and "Text" in item:
                t=item["Text"]
                u=item.get("FirstURL","").replace("https://duckduckgo.com/","ddg/")
                results.append({"t":short(t.split(".")[0] if "." in t[:50] else t),"s":clean(t),"u":short(u,28)})
        return results
    except Exception as e:
        print(f"DDG error: {e}")
        return []

@app.route("/")
def index():
    return "xyz Search Proxy v2.0 - OK", 200

@app.route("/ping")
def ping():
    return "pong", 200

@app.route("/search")
def search():
    query = request.args.get("q","").strip()
    if not query:
        return Response('{"count":0}', mimetype='application/json')

    print(f"[Search] '{query}'")
    results = wiki_search(query)
    if not results:
        results = ddg_search(query)

    # Build response manually as a simple flat string - easier for ESP32 to parse
    # Format: count|title0|snippet0|url0|title1|snippet1|url1|...
    # Using pipe separator - guaranteed no pipes in Wikipedia text
    parts = [str(len(results))]
    for res in results[:5]:
        parts.append(res["t"])
        parts.append(res["s"])
        parts.append(res["u"])

    response_text = "|".join(parts)
    print(f"[Search] Returning {len(results)} results, len={len(response_text)}")
    return Response(response_text, mimetype='text/plain')

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
