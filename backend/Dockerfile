FROM python:3.12-slim-bookworm
WORKDIR /app
RUN pip install --no-cache-dir psycopg2-binary==2.9.10
COPY server.py model.py ./
USER 10001:10001
EXPOSE 8443
CMD ["python", "-u", "server.py"]
