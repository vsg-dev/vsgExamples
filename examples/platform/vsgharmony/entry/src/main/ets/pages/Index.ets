import { hilog } from '@kit.PerformanceAnalysisKit';
import vsgNapi from 'libentry.so';

const DOMAIN = 0x0000;

@Entry
@Component
struct Index {
  @State message: string = 'Hello Harmony VSG';
  xcomponentId: string = 'vsg' + (new Date().getTime());
  build() {
    Row() {
      Column() {
        Text(this.message)
          .fontSize($r('app.float.page_text_font_size'))
          .fontWeight(FontWeight.Bold)
        XComponent({
          type: XComponentType.SURFACE
        })
          .id(this.xcomponentId)
          .onAttach(() => {
            let node = this.getUIContext().getFrameNodeById(this.xcomponentId)
            vsgNapi.bindNode(node)
          })
          .onDetach(() => {
            vsgNapi.unbindNode()
          })
          .width('100%')
          .height('100%')
          .focusable(true)
          .focusOnTouch(true)
          .defaultFocus(true)
      }
      .width('100%')
    }
    .height('100%')
  }
}
