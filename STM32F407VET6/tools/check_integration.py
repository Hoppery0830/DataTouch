"""Read-only checks, useful after CubeMX regeneration; not a substitute for rebuilding."""
from pathlib import Path
import re
r=Path(__file__).resolve().parents[1]
def read(name):return (r/name).read_text(encoding='utf-8')
def inside(name,tag,required):
    s=read(name);a=f'/* USER CODE BEGIN {tag} */';b=f'/* USER CODE END {tag} */'
    assert required in s.split(a,1)[1].split(b,1)[0],(name,tag,required)
inside('Core/Src/main.c','Includes','#include "board.h"')
inside('Core/Src/main.c','2','Board_Init();')
inside('Core/Src/main.c','Callback 1','Board_TimerCallback(htim);')
inside('Core/Src/freertos.c','Init','App_RTOS_Objects_Init();')
inside('Core/Src/freertos.c','Includes','#include "app_rtos.h"')
inside('Core/Src/freertos.c','RTOS_THREADS','!MotorTaskHandle || !ServiceTaskHandle')
inside('Core/Inc/FreeRTOSConfig.h','Defines','configCHECK_FOR_STACK_OVERFLOW 2')
inside('Core/Inc/FreeRTOSConfig.h','Defines','configUSE_MALLOC_FAILED_HOOK 1')
s=read('DataTouchCode.ioc');pairs=[l.split('=',1) for l in s.splitlines() if '=' in l and not l.startswith('#')]
keys=[p[0] for p in pairs];assert len(keys)==len(set(keys)), 'Duplicate ioc key'
d=dict(pairs);assert d['ProjectManager.KeepUserCode']=='true'
assert 'ServiceTask,8,512,StartServiceTask,As weak,NULL,Static' in d['FREERTOS.Tasks01']
assert d['TIM2.Prescaler']=='83' and d['TIM2.Period']=='1999'
assert d['NVIC.TIM2_IRQn'].startswith(r'true\:6\:0')
s=read('Core/Src/freertos.c')
assert re.search(r'__weak\s+void\s+StartMotorTask',s)
assert re.search(r'__weak\s+void\s+StartServiceTask',s)
assert '.cb_mem = &ServiceTaskControlBlock' in s
assert 'add_subdirectory(source)' in read('CMakeLists.txt')
s=read('Core/Src/tim.c');assert 'htim2.Init.Prescaler = 83;' in s and 'htim2.Init.Period = 1999;' in s
assert 'HAL_NVIC_EnableIRQ(TIM2_IRQn)' in s
assert 'void TIM2_IRQHandler(void)' in read('Core/Src/stm32f4xx_it.c')
print('PASS: ioc, static/Weak tasks, TIM2, USER CODE hooks and source build registration')

# Selftest must be advanced by the normal dispatcher, never block before the task loop.
s=read('source/task/Motor_Task/motor_task.c')
assert 'selftest_run(' not in s and 'selftest_wait_ready(' not in s
assert s.index('SelfTest_Update(')>s.index('for (;;)',s.index('void StartMotorTask'))
assert 'SelfTest_CommandResult(&selftest, ok, now)' in s
assert 'SelfTest_Cancel(&selftest)' in s
print('PASS: nonblocking selftest integration and command-result/stop hooks')

# Remote transport must survive regeneration without a second IRQ definition.
assert d['NVIC.USART3_IRQn'].startswith(r'true\:6\:0')
assert read('Core/Src/stm32f4xx_it.c').count('void USART3_IRQHandler(void)') == 1
assert 'RemoteTask_Init();' in read('source/task/app_rtos.c')
assert 'Remote_Update(&remote' in read('source/task/Motor_Task/motor_task.c')
assert 'module/remote/remote_control.c' in read('source/CMakeLists.txt')
print('PASS: USART3 IRQ, static RemoteTask and MotorTask remote controller integration')
