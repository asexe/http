//根据id获取元素
const cotainer = document.getElementById('container');
//窗口的鼠标移动事件，传入event对象
window.onmousemove = function(e){
  //返回被触发时的鼠标X,Y位置
  let x = e.clientX / 5
      y = e.clientY / 5;
      //将容器的背景图片定位进行修改
      container.style.backgroundPositionX = x + 'px';
      container.style.backgroundPositionY = y + 'px';
}
