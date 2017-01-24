FROM gcr.io/google_appengine/nodejs:latest
EXPOSE 80
RUN /usr/local/bin/install_node '>=7.4.0'
RUN node --version
COPY . /app/
RUN cd /app && npm install --unsafe-perm
CMD cd /app && npm run start

