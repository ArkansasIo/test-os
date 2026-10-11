# Manual smoke-test checklist

- [ ] Open `index.html` directly or through a local static server.
- [ ] Confirm the AetherOS Nexus logo and favicon load.
- [ ] Confirm POST rows transition from WAIT/CHECK to OK/READY.
- [ ] Confirm progress reaches 100% and the status identifies the sequence as simulated.
- [ ] Confirm clock and date update.
- [ ] Submit with an empty user name; it should request a user name.
- [ ] Submit with a user name; it should state authentication is not connected and clear the password field.
- [ ] Confirm session selector and shutdown/restart/accessibility buttons show safe demo messages.
- [ ] Check desktop and narrow mobile viewports.
- [ ] Check keyboard focus and reduced-motion behavior.
