# Docker Compose

Ubuntu and Docker share the same source revision on the Mumble-Funk `stable`
candidate, with Simplex Engine v0.2 on Mumble 1.5.939. Audio Feedback is excluded.

- [Docker installation and operations](funk/docker.md)
- [Safe migration of an existing Docker project](funk/docker-stable-migration.md)
- [Common integration verification](funk/STABLE-INTEGRATION-STATUS.md)

The previous `feature/docker-install` and `funk-v0.1` branches remain unchanged.
Preserve the existing `.env`, Compose project name and named volumes. The old
upstream Compose instructions referred to files absent from this fork and did not
describe its separate persistent configuration.
