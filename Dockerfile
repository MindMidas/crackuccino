FROM node:22-bookworm-slim AS runtime

ENV APP_BASE_PATH=/crackuccino/ \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

WORKDIR /app
RUN apt-get update \
    && apt-get install -y --no-install-recommends gcc make openmpi-bin libopenmpi-dev python3 \
    && rm -rf /var/lib/apt/lists/* \
    && addgroup --system crackuccino \
    && adduser --system --ingroup crackuccino crackuccino

COPY makefile ./
COPY src/ src/
COPY run_all.py ./
COPY src/frontend/package.json src/frontend/package-lock.json src/frontend/
RUN cd src/frontend && npm ci --no-audit --no-fund
COPY src/frontend/ src/frontend/
RUN make clean all \
    && cd src/frontend \
    && npm run build \
    && cd /app \
    && chown -R crackuccino:crackuccino /app

USER crackuccino
EXPOSE 8010

CMD ["python3", "-m", "src.server.app", "--host", "0.0.0.0", "--port", "8010"]
