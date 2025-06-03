from PIL import Image, ImageDraw, ImageFont
import os

# 确保resources目录存在
if not os.path.exists('resources'):
    os.makedirs('resources')

# 创建CPU图标
def create_cpu_icon():
    img = Image.new('RGBA', (150, 100), color=(0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 主体 - 蓝色渐变
    for y in range(10, 90):
        # 从顶部到底部的渐变
        factor = (y - 10) / 80.0
        r = int(65 + factor * 0)
        g = int(105 + factor * 0)
        b = int(225 - factor * 40)
        draw.line((10, y, 140, y), fill=(r, g, b))
    
    # 边框
    draw.rectangle((10, 10, 140, 90), outline=(0, 0, 0), width=1)
    
    # CPU内部结构
    draw.rectangle((20, 20, 70, 80), outline=(255, 255, 255), width=1)
    draw.rectangle((80, 20, 130, 45), outline=(200, 200, 200), width=1)
    draw.rectangle((80, 55, 130, 80), outline=(200, 200, 200), width=1)
    
    # 文本标签
    draw.text((45, 50), "Core", fill=(255, 255, 255))
    draw.text((105, 30), "L1", fill=(255, 255, 255))
    draw.text((105, 65), "L2", fill=(255, 255, 255))
    
    # 针脚
    for i in range(20, 140, 10):
        draw.line((i, 90, i, 95), fill=(100, 100, 100))
    
    img.save('resources/cpu.png')

# 创建L2缓存图标
def create_l2cache_icon():
    img = Image.new('RGBA', (150, 100), color=(0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 主体 - 绿色渐变
    for y in range(10, 90):
        # 从顶部到底部的渐变
        factor = (y - 10) / 80.0
        r = int(60 - factor * 20)
        g = int(179 - factor * 40)
        b = int(113 - factor * 30)
        draw.line((10, y, 140, y), fill=(r, g, b))
    
    # 边框
    draw.rectangle((10, 10, 140, 90), outline=(0, 0, 0), width=1)
    
    # 缓存单元
    for row in range(4):
        for col in range(6):
            x = 20 + col * 20
            y = 20 + row * 18
            draw.rectangle((x, y, x+16, y+14), outline=(255, 255, 255), width=1)
            
            # 一些单元格填充为亮色，表示有数据
            if (row + col) % 3 == 0:
                draw.rectangle((x+1, y+1, x+15, y+13), fill=(255, 255, 255, 50))
    
    img.save('resources/l2cache.png')

# 创建L3缓存图标
def create_l3cache_icon():
    img = Image.new('RGBA', (150, 100), color=(0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 主体 - 紫色渐变
    for y in range(10, 90):
        # 从顶部到底部的渐变
        factor = (y - 10) / 80.0
        r = int(106 - factor * 20)
        g = int(90 - factor * 20)
        b = int(205 - factor * 30)
        draw.line((10, y, 140, y), fill=(r, g, b))
    
    # 边框
    draw.rectangle((10, 10, 140, 90), outline=(0, 0, 0), width=1)
    
    # 缓存单元 - 更多更小的单元格
    for row in range(5):
        for col in range(8):
            x = 15 + col * 15
            y = 15 + row * 15
            draw.rectangle((x, y, x+12, y+12), outline=(255, 255, 255), width=1)
            
            # 一些单元格填充为亮色，表示有数据
            if (row + col) % 4 == 0:
                draw.rectangle((x+1, y+1, x+11, y+11), fill=(255, 255, 255, 50))
    
    img.save('resources/l3cache.png')

# 创建总线图标
def create_bus_icon():
    img = Image.new('RGBA', (150, 100), color=(0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 主体 - 橙色渐变
    for y in range(10, 90):
        # 从顶部到底部的渐变
        factor = (y - 10) / 80.0
        r = int(255 - factor * 30)
        g = int(165 - factor * 30)
        b = int(0)
        draw.line((10, y, 140, y), fill=(r, g, b))
    
    # 边框
    draw.rectangle((10, 10, 140, 90), outline=(0, 0, 0), width=1)
    
    # 总线线路
    for i in range(4):
        y = 25 + i * 15
        draw.line((20, y, 130, y), fill=(255, 255, 255), width=2)
    
    # 垂直连接线
    for i in range(5):
        x = 30 + i * 20
        draw.line((x, 25, x, 70), fill=(255, 255, 255), width=2)
    
    # 连接点
    for i in range(4):
        for j in range(5):
            x = 30 + j * 20
            y = 25 + i * 15
            draw.ellipse((x-3, y-3, x+3, y+3), fill=(255, 255, 255))
    
    img.save('resources/bus.png')

# 创建内存控制器图标
def create_memory_icon():
    img = Image.new('RGBA', (150, 100), color=(0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 主体 - 紫色渐变
    for y in range(10, 90):
        # 从顶部到底部的渐变
        factor = (y - 10) / 80.0
        r = int(186 - factor * 30)
        g = int(85 - factor * 20)
        b = int(211 - factor * 30)
        draw.line((10, y, 140, y), fill=(r, g, b))
    
    # 边框
    draw.rectangle((10, 10, 140, 90), outline=(0, 0, 0), width=1)
    
    # 内存条
    draw.rectangle((25, 20, 125, 80), fill=(220, 220, 220), outline=(255, 255, 255))
    
    # 内存芯片
    for i in range(4):
        for j in range(2):
            x = 35 + i * 25
            y = 30 + j * 30
            draw.rectangle((x, y, x+15, y+15), fill=(50, 50, 50))
    
    # 金属触点
    for i in range(20):
        x = 30 + i * 5
        draw.line((x, 80, x, 85), fill=(212, 175, 55), width=2)
    
    img.save('resources/memory.png')

# 创建DMA控制器图标
def create_dma_icon():
    img = Image.new('RGBA', (150, 100), color=(0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 主体 - 蓝色渐变
    for y in range(10, 90):
        # 从顶部到底部的渐变
        factor = (y - 10) / 80.0
        r = int(30)
        g = int(144 - factor * 30)
        b = int(255 - factor * 40)
        draw.line((10, y, 140, y), fill=(r, g, b))
    
    # 边框
    draw.rectangle((10, 10, 140, 90), outline=(0, 0, 0), width=1)
    
    # 控制器主体
    draw.rectangle((30, 20, 120, 80), outline=(255, 255, 255))
    
    # 数据流箭头
    draw.line((30, 40, 120, 40), fill=(255, 255, 255), width=2)
    draw.line((120, 60, 30, 60), fill=(255, 255, 255), width=2)
    
    # 箭头头部
    draw.polygon([(110, 35), (120, 40), (110, 45)], fill=(255, 255, 255))
    draw.polygon([(40, 55), (30, 60), (40, 65)], fill=(255, 255, 255))
    
    # 文本标签
    draw.text((60, 30), "DMA", fill=(255, 255, 255))
    
    img.save('resources/dma.png')

# 创建事件追踪器图标
def create_tracer_icon():
    img = Image.new('RGBA', (150, 100), color=(0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 主体 - 红色渐变
    for y in range(10, 90):
        # 从顶部到底部的渐变
        factor = (y - 10) / 80.0
        r = int(250 - factor * 40)
        g = int(128 - factor * 30)
        b = int(114 - factor * 30)
        draw.line((10, y, 140, y), fill=(r, g, b))
    
    # 边框
    draw.rectangle((10, 10, 140, 90), outline=(0, 0, 0), width=1)
    
    # 监视器屏幕
    draw.rectangle((25, 15, 125, 85), fill=(20, 20, 20), outline=(255, 255, 255))
    
    # 坐标轴
    draw.line((25, 50, 125, 50), fill=(100, 100, 100), width=1)  # X轴
    draw.line((25, 15, 25, 85), fill=(100, 100, 100), width=1)   # Y轴
    
    # 图表曲线
    points = [(25, 50), (40, 40), (55, 60), (70, 30), (85, 70), (100, 20), (125, 50)]
    for i in range(len(points)-1):
        draw.line([points[i], points[i+1]], fill=(0, 255, 0), width=2)
    
    img.save('resources/tracer.png')

# 创建所有图标
create_cpu_icon()
create_l2cache_icon()
create_l3cache_icon()
create_bus_icon()
create_memory_icon()
create_dma_icon()
create_tracer_icon()

print("图标文件生成完成！") 