FROM python:alpine

COPY requirements.txt /
COPY wheels /wheels
RUN pip install -f wheels/ -r requirements.txt

ENTRYPOINT ["python3", "/SigFoxFlask.py"]

COPY SigFoxFlask.py conversions.py /