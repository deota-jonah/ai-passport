[简体中文](deotaland-agent-link.zh_CN.md)

# Deotaland Agent Link firmware

This application branch turns AI Passport into a Wi-Fi voice terminal for the
Agent bound to its device in the [Deotaland console](https://bot.deotaland.cn/#/devices).
The hardware-test demo sources remain available as references; the application
entry point is `main/agent_link_app/passport_main.cc`.

## Setup and controls

1. On first boot, join `DeotaPassport-XXXX` from a phone and open
   `http://192.168.4.1`. Enter a 2.4 GHz Wi-Fi network and its password.
2. After joining the router, the setup hotspot closes. The screen displays a
   six-digit device identification/activation code and its remaining lifetime.
3. Open the console's Devices page, add the device using that code, then bind a
   voice-capable Agent. No account API key is compiled into the firmware.
4. Press OK to continue the latest conversation or create the first conversation.
   Press OK again to end the call. UP/DOWN change speaker volume.
5. Hold OK for conversations/settings. UP/DOWN select and OK confirms. Changing
   Wi-Fi requires a second confirmation, erases only network settings, and restarts
   into provisioning. Platform ownership and Agent binding are retained.
6. In Settings, choose Language and then Chinese or English. The entire fixed UI
   switches together; this preference is saved in NVS. New provisioning pages use
   the same language. Network names, addresses and Agent-provided content retain
   their original values.

Wi-Fi credentials are saved only after receiving an IP address. A failed join is
bounded to 20 seconds and returns to setup. Activation codes refresh after expiry.
The device auth key, identity, selected conversation and volume persist in NVS;
normal restarts reconnect automatically. A revoked credential initiates activation
again. An already-owned device must first be unbound in the console. Binding a
different Agent ends the old call and clears its selected conversation.

The screen shows battery state at the top right (`--` when unavailable). Its new
onboarding cards, activation code, status and conversations are drawn through the
official BSP panel using one 1,920-byte DMA stripe. LVGL is not started. Fonts
remain in Flash. Replies use an 8 KB PCM buffer; voice is half duplex, with the
microphone suppressed while the speaker and its echo tail are active. MQTT and
BLE are disabled to preserve RAM; this variant implements HTTP activation,
Agent/conversation APIs and Opus realtime WSS voice. It does not advertise MQTT
remote control or OTA support. TLS certificate verification remains enabled.

## Build and verification

Upstream targets ESP-IDF 5.5.3. The user authorized this local development build
with the existing ESP-IDF 5.5.4 installation. The installed SDK reports
`v5.5.4-dirty`; its pre-existing modifications were not changed by this application.
The BSP's LVGL dependency is pinned to the original 9.5.0 to prevent an unrelated
upgrade when adding the audio codec dependency.

In an activated ESP-IDF terminal, configure once and build with at most two jobs:

```text
idf.py -B build reconfigure
ninja -C build -j 2
idf.py -B build merge-bin -o FoloToy-AI-Passport-full.bin
python tools/verify_firmware.py build
python tools/archive_firmware.py create build --archive-root build/firmware
python tools/test_agent_link.py
```

Run `tools/validate.sh` in a compatible shell with a host C compiler and
actionlint as required by [the upstream gate](engineering/build-and-test.md).
The extra screen test executes the actual application renderer, checks every
non-ASCII code point in the UI sources against the linked font, checks UTF-8
truncation, stored-language decoding, complete translation tables, language
separation and playback buffering, and renders setup/binding/expired/ready and
language-settings screens in both languages.
The font covers the imported GB2312 character set, not arbitrary Unicode;
unsupported glyphs retain a visible placeholder.

The merged image is for `0x0` on this ESP32-C3/8 MB device. A merged flash may reset
NVS and require setup and activation again. Segmented flashing with the compatible
partition table can preserve NVS. Building does not authorize flashing or a full
erase; follow the [data policy](engineering/firmware-layout.md#flashing-and-stored-data).

Before submission, verify actual startup, phone provisioning, wrong password and
router outage recovery, code expiry, claim/binding, reboot persistence, voice
input/output, Agent rebinding, Wi-Fi change and battery/UI rendering. Record the
board revision, build identity and observed results. Host previews and a build do
not establish hardware acceptance. ESP-IDF 5.5.3 CI remains a separate requirement.

## Sources and submission

The baseline is FoloToy/ai-passport commit
`006c2279852e352a8433b2c21e5e09be2ca36f77`. The MIT Agent Link SDK is vendored from
[Deotaland/Agent_link](https://github.com/Deotaland/Agent_link/tree/9f664fc028a664a47789506802aecfb0b69b7e18/firmware/components/agent_link)
commit `9f664fc028a664a47789506802aecfb0b69b7e18`, with its original documentation
and [license](../../components/agent_link/LICENSE). Local SDK changes remove automatic whole-NVS erasure on initialization errors
and add an optional bounded portal-language hook plus localized page templates. Application voice,
session, raster and playback logic derives from that same revision; board drivers
are replaced with official BSP adapters and onboarding/navigation are adapted here.

Raster Montserrat 14/20/28 sources retain the LVGL MIT notices; their optional
sparse-base field is explicitly initialized to zero. The 14 px CJK raster
font is imported unchanged from that revision, generated with
`firmware/boards/common/fonts/make_raster_cjk.py` from its `cjk_font.bin`; its
[SIL Open Font License](../../assets/fonts/agent_link/OFL.txt) is retained.
All generated glyph sources compile once from `assets/fonts/agent_link/`.

The official [community market workflow](release/project-completion.md#action-a)
and [contribution rules](../../.github/CONTRIBUTING.md) are distinct from this local
implementation. Submit the verified firmware as an application after device
acceptance; an upstream source PR needs maintainer review of this application
scope. Include the bilingual description, cover/device screenshots, setup URL,
capabilities, limitations, firmware hash and test results. No commit, push, PR,
firmware upload or market publication is performed by building this branch.

## Local validation limitation

On this Windows host, the upstream skill-installation test suite has three
failures caused by `WinError 1314` (directory symlink creation is not permitted).
The failures are retained and are unrelated to firmware behavior. The preceding
BSP/runtime and repository/archive tests passed; the bilingual application tests
run separately. The full static gate must be rerun on a host with symlink support.

## Build record

- Version: `0.1.0`, ESP-IDF `v5.5.4-dirty`, ESP32-C3, 8 MB Flash.
- Build: PASS; upstream firmware gate and archive re-verification passed.
- Host tests: bilingual application, glyphs, language separation, playback,
  repository and workflows passed; the complete static gate retains the three
  Windows permission failures above.
- Device tests: NOT RUN; read-only discovery found an Espressif USB candidate at `COM113`.
- Unverified: device provisioning, claiming, voice, reboot/language persistence,
  and ESP-IDF 5.5.3 CI.

```text
Archive: build/firmware/1125fd166dbad8ffebc91ff5064b69ba795d6c2560c5c4036291cb96cbe945b8
Full image SHA-256: 1125fd166dbad8ffebc91ff5064b69ba795d6c2560c5c4036291cb96cbe945b8
Matching ELF SHA-256: aad82f1d944288e2f02905a87d8034cf04e00e02c34a1ac06d5ab2b1b960f6d0
```

## Known limitation: voice recovery for an existing conversation

A subsequent user-supplied device log matches this build's ELF identity and
confirms startup, network connection, platform authentication, and voice exchange
after creating a conversation. An existing conversation may remain marked as
having a live realtime connection. The platform wraps `realtime_connection_live`
in an HTTP 200 response with business code 1002, while the current firmware
triggers recovery only for an actual HTTP 409 response. This case therefore fails.
Choose an unoccupied conversation or create one as a workaround. The recovery fix
is pending user approval and is not included in this submission. The historical
build record above retains its original device-test status; the remaining
on-device acceptance checks still need completion.
